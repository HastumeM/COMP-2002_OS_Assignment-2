/**
 * This file implements parallel mergesort.
 */

#include <stdio.h>
#include <string.h> /* for memcpy */
#include <stdlib.h> /* for malloc */
#include "mergesort.h"

/* Qimu Liu a1898827 */
/* this function will be called by mergesort() and also by parallel_mergesort(). */
void merge(int leftstart, int leftend, int rightstart, int rightend)
{
    int i = leftstart;
    int j = rightstart;
    int k = leftstart;

    while (i <= leftend && j <= rightend) {
        if (A[i] <= A[j]) {
            B[k++] = A[i++];
        } else {
            B[k++] = A[j++];
        }
    }
    while (i <= leftend) {
        B[k++] = A[i++];
    }
    while (j <= rightend) {
        B[k++] = A[j++];
    }
    /* Each merge owns this interval in both A and B. */
    memcpy(A + leftstart, B + leftstart,
           (size_t)(rightend - leftstart + 1) * sizeof *A);
}

/* this function will be called by parallel_mergesort() as its base case. */
void my_mergesort(int left, int right)
{
    int mid;
    if (left >= right) {
        return;
    }
    mid = left + (right - left) / 2;
    my_mergesort(left, mid);
    my_mergesort(mid + 1, right);
    merge(left, mid, mid + 1, right);
}

// Wujiacheng Ding a1939784
// Without a successful join, merging/freeing would be unsafe
static void join_or_exit(pthread_t thread)
{
    int status = pthread_join(thread, NULL);
    if (status != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(status));
        exit(EXIT_FAILURE);
    }
}

/* this function will be called by the testing program. */
void *parallel_mergesort(void *arg)
{
    const struct argument *args = arg;
    int left = args->left;
    int right = args->right;
    int level = args->level;
    int mid;
    int left_status;
    int right_status;
    pthread_t left_thread;
    pthread_t right_thread;
    struct argument *left_args;
    struct argument *right_args;

    if (left >= right) {
        return NULL;
    }
    if (level >= cutoff) {
        my_mergesort(left, right);
        return NULL;
    }
    mid = left + (right - left) / 2;
    left_args = buildArgs(left, mid, level + 1);
    right_args = buildArgs(mid + 1, right, level + 1);
    left_status = pthread_create(&left_thread, NULL, parallel_mergesort,
                                 left_args);
    right_status = pthread_create(&right_thread, NULL, parallel_mergesort,
                                  right_args);
    // Resource exhaustion must not leave either half unsorted
    if (left_status != 0) {
        my_mergesort(left, mid);
    }
    if (right_status != 0) {
        my_mergesort(mid + 1, right);
    }
    if (left_status == 0) {
        join_or_exit(left_thread);
    }
    if (right_status == 0) {
        join_or_exit(right_thread);
    }
    free(left_args);
    free(right_args);
    merge(left, mid, mid + 1, right);
    return NULL;
}

/* we build the argument for the parallel_mergesort function. */
struct argument *buildArgs(int left, int right, int level)
{
    struct argument *args = malloc(sizeof *args);
    if (args == NULL) {
        perror("malloc argument");
        exit(EXIT_FAILURE);
    }
    args->left = left;
    args->right = right;
    args->level = level;
    return args;
}
