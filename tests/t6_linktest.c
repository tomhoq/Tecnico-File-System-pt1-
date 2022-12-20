#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

//1078-34 = 1044 b
uint8_t const file_contents[] = "!!!!!!!!!!";
char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const target_path2[] = "/f2";
char const link_path2[] = "/l2";

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void assert_empty_file(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

int main() {
    
    assert(tfs_init(NULL) != -1);
    //Creates tgpath1
    int f = tfs_open(target_path1, 0);
    //Creates lnkpath1
    int g = tfs_open(link_path1, 0);
    assert(tfs_close(f) != -1);
    assert(tfs_close(g) != -1);
    assert(f != -1);
    assert(tfs_link(target_path2, link_path2) == -1);       //  error target_path2 doesnt exist o
    assert(tfs_sym_link(target_path2, link_path2) == -1);   //  error target_path2 doesnt exist
    assert(tfs_link(target_path1, link_path1) == -1);       //  error link_path1 already exists
    assert(tfs_link(target_path1, link_path2) != -1);       //  creates link  to target path1
    // linkpath2 is a hard link of targetpath1
    assert(tfs_sym_link(target_path1, link_path1) != -1);   //  creates sym link to target path 1
    // link path1 is a sym link of target path1
    assert(tfs_link(link_path1, link_path2) != -1);           // tries to create a hard link of a sym link    
    

    printf("FINISH.\n");
    
    return 0;
}
