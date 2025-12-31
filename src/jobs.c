#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "jobs.h"
#include "colors.h"
#include "safe_string.h"
#include "hash.h"

// Job table
static Job jobs[MAX_JOBS];
static int next_job_id = 1;
static int current_job = 0;  // Most recent job

// Initialize job control
void jobs_init(void) {
    // Clear job table
    memset(jobs, 0, sizeof(jobs));
    next_job_id = 1;
    current_job = 0;

    // Set up SIGCHLD handler
    struct sigaction sa;
    sa.sa_handler = jobs_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
    }
}

// Find an empty slot in the job table
static int find_empty_slot(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            return i;
        }
    }
    return -1;
}

// Add a new background job
int jobs_add(pid_t pid, const char *command) {
    int slot = find_empty_slot();
    if (slot == -1) {
        color_error("%s: too many background jobs", HASH_NAME);
        return -1;
    }

    jobs[slot].job_id = next_job_id++;
    jobs[slot].pid = pid;
    jobs[slot].pgid = pid;  // For now, pgid = pid
    jobs[slot].state = JOB_RUNNING;
    jobs[slot].notified = false;

    if (command) {
        safe_strcpy(jobs[slot].command, command, MAX_JOB_CMD);
    } else {
        jobs[slot].command[0] = '\0';
    }

    current_job = jobs[slot].job_id;

    return jobs[slot].job_id;
}

// Remove a job by job ID
int jobs_remove(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id && jobs[i].pid != 0) {
            jobs[i].pid = 0;
            jobs[i].job_id = 0;
            jobs[i].command[0] = '\0';

            // Update current job if needed
            if (current_job == job_id) {
                current_job = 0;
                // Find next most recent job
                for (int j = MAX_JOBS - 1; j >= 0; j--) {
                    if (jobs[j].pid != 0 && jobs[j].state == JOB_RUNNING) {
                        current_job = jobs[j].job_id;
                        break;
                    }
                }
            }

            return 0;
        }
    }
    return -1;
}

// Get job by job ID
Job *jobs_get(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id && jobs[i].pid != 0) {
            return &jobs[i];
        }
    }
    return NULL;
}

// Get job by PID
Job *jobs_get_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return NULL;
}

// Get the most recent job
Job *jobs_get_current(void) {
    if (current_job == 0) return NULL;
    return jobs_get(current_job);
}

// Update job state based on waitpid status
void jobs_update_status(pid_t pid, int status) {
    Job *job = jobs_get_by_pid(pid);
    if (!job) return;

    if (WIFEXITED(status)) {
        job->state = JOB_DONE;
    } else if (WIFSIGNALED(status)) {
        job->state = JOB_TERMINATED;
    } else if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED;
    }
}

// Get state string
static const char *state_string(JobState state) {
    switch (state) {
        case JOB_RUNNING:    return "Running";
        case JOB_STOPPED:    return "Stopped";
        case JOB_DONE:       return "Done";
        case JOB_TERMINATED: return "Terminated";
        default:             return "Unknown";
    }
}

// Check for and report completed background jobs
void jobs_check_completed(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0 && !jobs[i].notified) {
            // Check if process has terminated
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);

            if (result > 0) {
                // Process has terminated
                jobs_update_status(jobs[i].pid, status);
                jobs[i].notified = true;

                // Print notification
                printf("[%d]  %s\t\t%s\n",
                    jobs[i].job_id,
                    state_string(jobs[i].state),
                    jobs[i].command);

                // Remove completed job
                if (jobs[i].state == JOB_DONE || jobs[i].state == JOB_TERMINATED) {
                    jobs_remove(jobs[i].job_id);
                }
            } else if (result == -1 && errno == ECHILD) {
                // Process doesn't exist anymore
                jobs[i].state = JOB_DONE;
                jobs[i].notified = true;
                printf("[%d]  %s\t\t%s\n",
                    jobs[i].job_id,
                    state_string(jobs[i].state),
                    jobs[i].command);
                jobs_remove(jobs[i].job_id);
            }
        }
    }
}

// List all jobs
void jobs_list(void) {
    int found = 0;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0) {
            // Check current status
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);

            if (result > 0) {
                jobs_update_status(jobs[i].pid, status);
            }

            // Print job info
            char current_marker = (jobs[i].job_id == current_job) ? '+' : '-';

            color_print(COLOR_CYAN, "[%d]%c ", jobs[i].job_id, current_marker);
            printf("%-12s  ", state_string(jobs[i].state));
            printf("%s", jobs[i].command);

            if (jobs[i].state == JOB_RUNNING) {
                color_print(COLOR_DIM, " &");
            }
            printf("\n");

            found++;
        }
    }

    if (found == 0) {
        printf("No jobs\n");
    }
}

// Get number of active jobs
int jobs_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0 &&
            (jobs[i].state == JOB_RUNNING || jobs[i].state == JOB_STOPPED)) {
            count++;
        }
    }
    return count;
}

// Wait for a specific job to complete
int jobs_wait(int job_id) {
    const Job *job = jobs_get(job_id);
    if (!job) {
        color_error("%s: job %d not found", HASH_NAME, job_id);
        return -1;
    }

    int status;
    pid_t result = waitpid(job->pid, &status, 0);

    if (result > 0) {
        jobs_update_status(job->pid, status);
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        jobs_remove(job_id);
        return exit_code;
    }

    return -1;
}

// Bring a job to foreground
int jobs_foreground(int job_id) {
    Job *job;

    if (job_id == 0) {
        job = jobs_get_current();
        if (!job) {
            color_error("%s: no current job", HASH_NAME);
            return -1;
        }
    } else {
        job = jobs_get(job_id);
        if (!job) {
            color_error("%s: job %d not found", HASH_NAME, job_id);
            return -1;
        }
    }

    // Print command being foregrounded
    printf("%s\n", job->command);

    // If job was stopped, continue it
    if (job->state == JOB_STOPPED) {
        if (kill(job->pid, SIGCONT) == -1) {
            perror("kill (SIGCONT)");
            return -1;
        }
        job->state = JOB_RUNNING;
    }

    // Wait for the job
    int status;
    pid_t result = waitpid(job->pid, &status, WUNTRACED);

    if (result > 0) {
        if (WIFSTOPPED(status)) {
            // Job was stopped (Ctrl+Z)
            job->state = JOB_STOPPED;
            printf("\n[%d]+  Stopped\t\t%s\n", job->job_id, job->command);
            return 0;
        } else {
            // Job completed
            jobs_update_status(job->pid, status);
            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            jobs_remove(job->job_id);
            return exit_code;
        }
    }

    return -1;
}

// Continue a stopped job in background
int jobs_background(int job_id) {
    Job *job;

    if (job_id == 0) {
        job = jobs_get_current();
        if (!job) {
            color_error("%s: no current job", HASH_NAME);
            return -1;
        }
    } else {
        job = jobs_get(job_id);
        if (!job) {
            color_error("%s: job %d not found", HASH_NAME, job_id);
            return -1;
        }
    }

    if (job->state != JOB_STOPPED) {
        color_error("%s: job %d is not stopped", HASH_NAME, job->job_id);
        return -1;
    }

    // Continue the job
    if (kill(job->pid, SIGCONT) == -1) {
        perror("kill (SIGCONT)");
        return -1;
    }

    job->state = JOB_RUNNING;
    printf("[%d]+ %s &\n", job->job_id, job->command);

    return 0;
}

// SIGCHLD handler
void jobs_sigchld_handler(int sig) {
    (void)sig;

    int saved_errno = errno;
    int status;
    pid_t pid;

    // Reap all terminated children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        const Job *job = jobs_get_by_pid(pid);
        if (job) {
            jobs_update_status(pid, status);
        }
    }

    errno = saved_errno;
}
