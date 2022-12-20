#include "fs/operations.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *path_copied_file = "/f1";
char *path_thread1 = "tests/input1.txt";
char *path_thread2 = "tests/input2.txt";
char *path_thread3 = "tests/input3.txt";
char *exp_output = "INPUT 1";
char buffer[50];

void *thread1() {
	int f = tfs_copy_from_external_fs(path_thread1, path_copied_file);
	assert(f != -1);
	return NULL;
}

void *thread2() {
	int f = tfs_copy_from_external_fs(path_thread1, path_copied_file);
	assert(f != -1);
	return NULL;
}

void *thread3() {
	int f = tfs_copy_from_external_fs(path_thread1, path_copied_file);
	assert(f != -1);
	return NULL;
}


int main() {
    assert(tfs_init(NULL) != -1);

	pthread_t t1, t2, t3;
	pthread_create(&t1, NULL, thread1, NULL);
	pthread_create(&t2, NULL, thread2, NULL);
	pthread_create(&t3, NULL, thread3, NULL);

	int f = tfs_open(path_copied_file, TFS_O_CREAT);
	assert(f != -1);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);

	ssize_t read = tfs_read(f, buffer, sizeof(buffer));
	assert(read == strlen(exp_output));
	//assert(strncmp(buffer, exp_output, strlen(exp_output)) == 0);

	printf("Successful test.\n");
	return 0;
}