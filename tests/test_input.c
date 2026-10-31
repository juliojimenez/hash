#include "unity.h"
#include "../src/input.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

// The read end of a pipe holding exactly `data`, write end already closed
static int pipe_with(const char *data) {
    int fds[2];
    TEST_ASSERT_EQUAL_INT(0, pipe(fds));
    size_t len = strlen(data);
    TEST_ASSERT_EQUAL_INT((int)len, (int)write(fds[1], data, len));
    close(fds[1]);
    return fds[0];
}

// An unlinked temporary file holding `data`, positioned at its start
static int file_with(const char *data) {
    char path[] = "/tmp/hash_test_input_XXXXXX";
    int fd = mkstemp(path);
    TEST_ASSERT_TRUE(fd >= 0);
    unlink(path);
    size_t len = strlen(data);
    TEST_ASSERT_EQUAL_INT((int)len, (int)write(fd, data, len));
    TEST_ASSERT_EQUAL_INT(0, (int)lseek(fd, 0, SEEK_SET));
    return fd;
}

void test_pipe_line_leaves_the_rest_for_the_next_reader(void) {
    int fd = pipe_with("first\nsecond\n");
    char buf[64];
    TEST_ASSERT_EQUAL_INT(6, (int)input_read_line(fd, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("first\n", buf);
    // Whatever follows is still in the pipe for whoever reads it next
    char rest[64] = {0};
    TEST_ASSERT_EQUAL_INT(7, (int)read(fd, rest, sizeof(rest) - 1));
    TEST_ASSERT_EQUAL_STRING("second\n", rest);
    TEST_ASSERT_EQUAL_INT(0, (int)input_read_line(fd, buf, sizeof(buf)));
    close(fd);
}

void test_file_is_repositioned_after_the_newline(void) {
    int fd = file_with("one\ntwo\nthree");
    char buf[64];
    TEST_ASSERT_EQUAL_INT(4, (int)input_read_line(fd, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("one\n", buf);
    TEST_ASSERT_EQUAL_INT(4, (int)lseek(fd, 0, SEEK_CUR));
    TEST_ASSERT_EQUAL_INT(4, (int)input_read_line(fd, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("two\n", buf);
    // A last line without newline, then end of input
    TEST_ASSERT_EQUAL_INT(5, (int)input_read_line(fd, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("three", buf);
    TEST_ASSERT_EQUAL_INT(0, (int)input_read_line(fd, buf, sizeof(buf)));
    close(fd);
}

void test_long_line_comes_back_in_pieces(void) {
    const char *data = "abcdefgh\nz\n";
    int sources[2] = {pipe_with(data), file_with(data)};
    for (int i = 0; i < 2; i++) {
        int fd = sources[i];
        char buf[5];
        TEST_ASSERT_EQUAL_INT(4, (int)input_read_line(fd, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_STRING("abcd", buf);
        TEST_ASSERT_EQUAL_INT(4, (int)input_read_line(fd, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_STRING("efgh", buf);
        TEST_ASSERT_EQUAL_INT(1, (int)input_read_line(fd, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_STRING("\n", buf);
        TEST_ASSERT_EQUAL_INT(2, (int)input_read_line(fd, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_STRING("z\n", buf);
        TEST_ASSERT_EQUAL_INT(0, (int)input_read_line(fd, buf, sizeof(buf)));
        close(fd);
    }
}

void test_alloc_reads_a_line_of_any_length(void) {
    size_t big_len = 10000;
    char *big = malloc(big_len + 2);
    TEST_ASSERT_NOT_NULL(big);
    memset(big, 'x', big_len);
    big[big_len] = '\n';
    big[big_len + 1] = '\0';
    int fd = file_with(big);
    size_t len = 0;
    char *line = input_read_line_alloc(fd, &len);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_EQUAL_INT((int)big_len + 1, (int)len);
    TEST_ASSERT_EQUAL_MEMORY(big, line, big_len + 1);
    free(line);
    // End of input: NULL with errno cleared
    errno = EIO;
    TEST_ASSERT_NULL(input_read_line_alloc(fd, &len));
    TEST_ASSERT_EQUAL_INT(0, errno);
    close(fd);
    free(big);
}

void test_alloc_keeps_a_last_line_without_newline(void) {
    int fd = pipe_with("tail");
    size_t len = 0;
    char *line = input_read_line_alloc(fd, &len);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_EQUAL_STRING("tail", line);
    TEST_ASSERT_EQUAL_INT(4, (int)len);
    free(line);
    TEST_ASSERT_NULL(input_read_line_alloc(fd, NULL));
    TEST_ASSERT_EQUAL_INT(0, errno);
    close(fd);
}

void test_read_error_is_reported(void) {
    char buf[8];
    errno = 0;
    TEST_ASSERT_EQUAL_INT(-1, (int)input_read_line(-1, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(EBADF, errno);
    TEST_ASSERT_EQUAL_STRING("", buf);
    errno = 0;
    TEST_ASSERT_NULL(input_read_line_alloc(-1, NULL));
    TEST_ASSERT_EQUAL_INT(EBADF, errno);
    errno = 0;
    TEST_ASSERT_EQUAL_INT(-1, (int)input_read_line(0, buf, 1));
    TEST_ASSERT_EQUAL_INT(EINVAL, errno);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pipe_line_leaves_the_rest_for_the_next_reader);
    RUN_TEST(test_file_is_repositioned_after_the_newline);
    RUN_TEST(test_long_line_comes_back_in_pieces);
    RUN_TEST(test_alloc_reads_a_line_of_any_length);
    RUN_TEST(test_alloc_keeps_a_last_line_without_newline);
    RUN_TEST(test_read_error_is_reported);
    return UNITY_END();
}
