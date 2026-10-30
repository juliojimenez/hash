#include "unity.h"
#include "../src/arena.h"
#include <stdint.h>
#include <string.h>

static Arena arena;

void setUp(void) {
    arena_init(&arena, 64);
}

void tearDown(void) {
    arena_free(&arena);
}

void test_alloc_returns_aligned_memory(void) {
    void *a = arena_alloc(&arena, 1);
    void *b = arena_alloc(&arena, 3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)a % ARENA_ALIGNMENT);
    TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)b % ARENA_ALIGNMENT);
    TEST_ASSERT_TRUE(a != b);
}

void test_zero_size_alloc_is_valid(void) {
    void *a = arena_alloc(&arena, 0);
    void *b = arena_alloc(&arena, 0);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a != b);
}

void test_allocations_do_not_overlap_across_chunks(void) {
    /* 64-byte chunks, 40-byte blocks: every other block opens a new chunk */
    char *blocks[32];
    for (int i = 0; i < 32; i++) {
        blocks[i] = arena_alloc(&arena, 40);
        TEST_ASSERT_NOT_NULL(blocks[i]);
        memset(blocks[i], 'a' + (i % 26), 40);
    }
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 40; j++) {
            TEST_ASSERT_EQUAL_CHAR('a' + (i % 26), blocks[i][j]);
        }
    }
}

void test_large_allocation_gets_its_own_chunk(void) {
    char *big = arena_alloc(&arena, 1000);
    TEST_ASSERT_NOT_NULL(big);
    memset(big, 'x', 1000);
    char *small = arena_alloc(&arena, 8);
    TEST_ASSERT_NOT_NULL(small);
    TEST_ASSERT_EQUAL_CHAR('x', big[999]);
}

void test_strndup_copies_and_terminates(void) {
    char *copy = arena_strndup(&arena, "hello world", 5);
    TEST_ASSERT_EQUAL_STRING("hello", copy);
    char *full = arena_strdup(&arena, "line\\\ncontinued");
    TEST_ASSERT_EQUAL_STRING("line\\\ncontinued", full);
}

void test_reset_rewinds_and_keeps_working(void) {
    for (int i = 0; i < 10; i++) {
        arena_alloc(&arena, 40);
    }
    TEST_ASSERT_TRUE(arena.used > 0);
    arena_reset(&arena);
    TEST_ASSERT_EQUAL_UINT(0, arena.used);
    TEST_ASSERT_NOT_NULL(arena.current);

    char *s = arena_strdup(&arena, "after reset");
    TEST_ASSERT_EQUAL_STRING("after reset", s);
    TEST_ASSERT_EQUAL_UINT(16, arena.used);
}

void test_reset_on_empty_arena_is_safe(void) {
    Arena empty;
    arena_init(&empty, 0);
    arena_reset(&empty);
    TEST_ASSERT_NULL(empty.current);
    arena_free(&empty);
}

void test_default_chunk_size(void) {
    Arena a;
    arena_init(&a, 0);
    TEST_ASSERT_EQUAL_UINT(ARENA_DEFAULT_CHUNK_SIZE, a.chunk_size);
    arena_free(&a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_alloc_returns_aligned_memory);
    RUN_TEST(test_zero_size_alloc_is_valid);
    RUN_TEST(test_allocations_do_not_overlap_across_chunks);
    RUN_TEST(test_large_allocation_gets_its_own_chunk);
    RUN_TEST(test_strndup_copies_and_terminates);
    RUN_TEST(test_reset_rewinds_and_keeps_working);
    RUN_TEST(test_reset_on_empty_arena_is_safe);
    RUN_TEST(test_default_chunk_size);
    return UNITY_END();
}
