#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/* global value */
int g_value = 0;

void* thr_func() {

    char *path = "/f1";;
    char *str = "AAA!";
    char buffer[40];
    int f;
    ssize_t r;
    
    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    assert(tfs_close(f) != -1);
    return NULL;
}

int main() {
    pthread_t tid;
    pthread_t tid2;

    assert(tfs_init(NULL) != -1);

    if (pthread_create(&tid, NULL, thr_func, NULL) != 0) {
        fprintf(stderr, "error creating thread.\n");
        return -1;
    }

    if (pthread_create(&tid2, NULL, thr_func, NULL) != 0) {
        fprintf(stderr, "error creating thread.\n");
        return -1;
    }
    
    if(pthread_join(tid, NULL) != 0) {
        fprintf(stderr, "error joining thread.\n");
        return -1;
    }

    if(pthread_join(tid2, NULL) != 0) {
        fprintf(stderr, "error joining thread.\n");
        return -1;
    }

    printf("Successful test.\n");

    return 0;
}
