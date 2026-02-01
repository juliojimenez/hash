#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_JOBS 64
#define MAX_JOB_CMD 256

// Job states
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
    JOB_TERMINATED
} JobState;

// Job structure
typedef struct {
    int job_id;           // Job number [1], [2], etc.
    pid_t pid;            // Process ID
    pid_t pgid;           // Process group ID
    JobState state;       // Current state
    int exit_status;      // Exit status (for wait after SIGCHLD reaps child)
    char command[MAX_JOB_CMD];  // Command string for display
    bool notified;        // Whether user has been notified of completion
} Job;

/**
 * Initialize job control system
 * Sets up signal handlers for SIGCHLD
 */
void jobs_init(void);

/**
 * Add a new background job
 *
 * @param pid Process ID of the job
 * @param command Command string for display
 * @return Job ID (1-based), or -1 on error
 */
int jobs_add(pid_t pid, const char *command);

/**
 * Remove a job by job ID
 *
 * @param job_id Job ID to remove
 * @return 0 on success, -1 if not found
 */
int jobs_remove(int job_id);

/**
 * Get job by job ID
 *
 * @param job_id Job ID
 * @return Pointer to job, or NULL if not found
 */
Job *jobs_get(int job_id);

/**
 * Get job by PID
 *
 * @param pid Process ID
 * @return Pointer to job, or NULL if not found
 */
Job *jobs_get_by_pid(pid_t pid);

/**
 * Get the most recent job (for fg/bg with no args)
 *
 * @return Pointer to most recent job, or NULL if none
 */
Job *jobs_get_current(void);

/**
 * Update job state based on waitpid status
 *
 * @param pid Process ID
 * @param status Status from waitpid
 */
void jobs_update_status(pid_t pid, int status);

/**
 * Check for and report completed background jobs
 * Called before each prompt
 */
void jobs_check_completed(void);

/**
 * Jobs output format
 */
typedef enum {
    JOBS_FORMAT_DEFAULT,  // Default format
    JOBS_FORMAT_LONG,     // -l: Include PID
    JOBS_FORMAT_PID_ONLY  // -p: Only show PIDs
} JobsFormat;

/**
 * List all jobs (for 'jobs' builtin)
 *
 * @param format Output format (default, long, or PID-only)
 */
void jobs_list(JobsFormat format);

/**
 * Get number of active jobs
 *
 * @return Number of running or stopped jobs
 */
int jobs_count(void);

/**
 * Wait for a specific job to complete
 *
 * @param job_id Job ID to wait for
 * @return Exit status of the job
 */
int jobs_wait(int job_id);

/**
 * Bring a job to foreground
 *
 * @param job_id Job ID (0 for current job)
 * @return 0 on success, -1 on error
 */
int jobs_foreground(int job_id);

/**
 * Continue a stopped job in background
 *
 * @param job_id Job ID (0 for current job)
 * @return 0 on success, -1 on error
 */
int jobs_background(int job_id);

/**
 * SIGCHLD handler - reaps zombie processes
 */
void jobs_sigchld_handler(int sig);

/**
 * Get the PID of the most recently started background job
 * Used for $! expansion
 *
 * @return PID of last background job, or 0 if none
 */
pid_t jobs_get_last_bg_pid(void);

/**
 * Set the PID of the most recently started background job
 * Called when a background job is started
 *
 * @param pid PID of the background job
 */
void jobs_set_last_bg_pid(pid_t pid);

#endif // JOBS_H
