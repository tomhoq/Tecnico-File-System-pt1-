#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
uint8_t const file_contents[] = "AAA!";
char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const target_path2[] = "/f2";
char const link_path2[] = "/l2";
char const new_path1[] = "/n1";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}
void *f1(){
    {
        int f = tfs_open(target_path1, TFS_O_CREAT);
        assert(f != -1);
        assert(tfs_link(target_path1, new_path1) != -1);
        assert(tfs_close(f) != -1);
        f = tfs_open(target_path1, TFS_O_CREAT);
        assert(tfs_unlink(target_path1) == -1);
        assert(tfs_close(f) != -1);
        assert(tfs_unlink(target_path1) != -1);
        assert(tfs_unlink(new_path1) != -1);
    }

    printf("Successful test1.\n");
    return NULL;
}
void *f2(){
    {
        int f = tfs_open(target_path1, TFS_O_CREAT);
        assert(f != -1);
        write_contents(target_path1);
        assert(tfs_link(target_path1, new_path1) != -1);
        assert(tfs_link(new_path1, new_path1) == -1);
        assert_contents_ok(new_path1);
        assert(tfs_sym_link(target_path1, link_path1) != -1);
        assert(tfs_link(link_path1, link_path2) == -1);
        assert(tfs_unlink(target_path1) == -1);
        assert_contents_ok(link_path1);
        assert(tfs_close(f) != -1);
        assert(tfs_unlink(link_path1) != -1);
        assert(tfs_unlink(target_path1) != -1);
        assert(tfs_unlink(new_path1) != -1);
        assert(tfs_unlink(link_path1) == -1);
    }

    printf("Successful test2.\n");
    return NULL;
}
int main() {
    pthread_t pid[6];
    
    assert(tfs_init(NULL) != -1);

    for (int i = 0; i < 3; i++) {
        pthread_create(&pid[i],NULL,(void *)f1,NULL);
        pthread_create(&pid[5-i],NULL,(void *)f2,NULL);
    }
    
    for (int i = 0; i < 6; i++) {
        pthread_join(pid[i],NULL);
    }

    printf("FINISH.\n");
    
    return 0;
}