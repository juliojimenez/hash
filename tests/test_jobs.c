#include "unity.h"
#include "../src/jobs.h"
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void setUp(void) {
    jobs_init();
}

void tearDown(void) {
    // Clean up any remaining jobs
}

// Test jobs initialization
void test_jobs_init(void) {
    jobs_init();
    TEST_ASSERT_EQUAL_INT(0, jobs_count());
}

// Test adding a job
void test_jobs_add(void) {
    // Fork a simple process that sleeps
    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);
        _exit(0);
    }

    int job_id = jobs_add(pid, "sleep 10");
    TEST_ASSERT_GREATER_THAN(0, job_id);
    TEST_ASSERT_EQUAL_INT(1, jobs_count());

    // Clean up
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

// Test getting job by ID
void test_jobs_get(void) {
    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);
        _exit(0);
    }

    int job_id = jobs_add(pid, "test command");
    Job *job = jobs_get(job_id);

    TEST_ASSERT_NOT_NULL(job);
    TEST_ASSERT_EQUAL_INT(pid, job->pid);
    TEST_ASSERT_EQUAL_STRING("test command", job->command);
    TEST_ASSERT_EQUAL_INT(JOB_RUNNING, job->state);

    // Clean up
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

// Test getting job by PID
void test_jobs_get_by_pid(void) {
    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);
        _exit(0);
    }

    int job_id = jobs_add(pid, "test");
    Job *job = jobs_get_by_pid(pid);

    TEST_ASSERT_NOT_NULL(job);
    TEST_ASSERT_EQUAL_INT(job_id, job->job_id);

    // Clean up
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

// Test removing a job
void test_jobs_remove(void) {
    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);
        _exit(0);
    }

    int job_id = jobs_add(pid, "test");
    TEST_ASSERT_EQUAL_INT(1, jobs_count());

    int result = jobs_remove(job_id);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(0, jobs_count());

    // Clean up
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

// Test getting non-existent job
void test_jobs_get_nonexistent(void) {
    Job *job = jobs_get(999);
    TEST_ASSERT_NULL(job);
}

// Test get current job
void test_jobs_get_current(void) {
    // Initially no current job
    Job *job = jobs_get_current();
    TEST_ASSERT_NULL(job);

    // Add a job
    pid_t pid = fork();
    if (pid == 0) {
        sleep(10);
        _exit(0);
    }

    jobs_add(pid, "current job");

    job = jobs_get_current();
    TEST_ASSERT_NOT_NULL(job);
    TEST_ASSERT_EQUAL_STRING("current job", job->command);

    // Clean up
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

// Test multiple jobs
void test_jobs_multiple(void) {
    pid_t pid1 = fork();
    if (pid1 == 0) {
        sleep(10);
        _exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        sleep(10);
        _exit(0);
    }

    int job_id1 = jobs_add(pid1, "job1");
    int job_id2 = jobs_add(pid2, "job2");

    TEST_ASSERT_EQUAL_INT(2, jobs_count());
    TEST_ASSERT_NOT_EQUAL(job_id1, job_id2);

    Job *j1 = jobs_get(job_id1);
    Job *j2 = jobs_get(job_id2);

    TEST_ASSERT_EQUAL_STRING("job1", j1->command);
    TEST_ASSERT_EQUAL_STRING("job2", j2->command);

    // Clean up
    kill(pid1, SIGKILL);
    kill(pid2, SIGKILL);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_jobs_init);
    RUN_TEST(test_jobs_add);
    RUN_TEST(test_jobs_get);
    RUN_TEST(test_jobs_get_by_pid);
    RUN_TEST(test_jobs_remove);
    RUN_TEST(test_jobs_get_nonexistent);
    RUN_TEST(test_jobs_get_current);
    RUN_TEST(test_jobs_multiple);

    return UNITY_END();
}
