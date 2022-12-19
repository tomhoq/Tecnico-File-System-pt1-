#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}
 // initializes tfs
int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {   
        params = *params_ptr;
    } else {
        params = tfs_default_params(); //if given no values makes values of tfs default
    }

    if (state_init(params) != 0) {     // initializes tables, returns 0 if successful
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY,false);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;
    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        while (inode->is_sym_link == true) {
            char* new = (char*) data_block_get(inode->i_data_block);
            if ((inum = tfs_lookup(new, root_dir_inode)) > 0)
                inode = inode_get(inum);
            else {
                return -1;
            }
        }
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE, false);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int sym_inumber = inode_create(T_FILE, true); //Criar inode do sym_link

    //Adicionar uma entrada no diretório onde está target_file com o nome target_file que é um soft_link
    if (add_dir_entry(root_dir_inode, ++link_name, sym_inumber) != 0) {
        return -1;
    }
    inode_t *sym_inode = inode_get(sym_inumber);

    sym_inode->i_data_block = data_block_alloc();
    char *sym_data_block = data_block_get(sym_inode->i_data_block);

    memcpy(sym_data_block, target, strlen(target)+1);   //Copiar o path de target para o bloco de dados do symlink

    sym_inode->i_size = state_block_size(); //Atribuir tamanho ao symlink, TALVEZ NÃO SEJA ESTE O TAMANHO

    //Quando se apaga o source_file, o soft_link vai ser quebrado, incorrendo em erros.
    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int target_inumber = tfs_lookup(target, root_dir_inode);
    if (tfs_lookup(link_name, root_dir_inode) != -1 || inode_get(target_inumber)->is_sym_link == true) { 
        return -1;  //Já existe um link com este nome / não é possível criar hard_links para sym_links
    }

    if (target_inumber != -1 && (add_dir_entry(root_dir_inode, ++link_name, target_inumber) == 0)) {
        //write lock
        inode_get(target_inumber)->hard_links++;
        return 0;
    }

    return -1;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    int inumber = find_in_dir(root_dir_inode, ++target);
    inode_t *t_inode = inode_get(inumber);
    
    t_inode->hard_links--;
    if (t_inode->is_sym_link) {
        //inode_t* origin_inode = inode_get(tfs_lookup((char*) data_block_get(t_inode->i_data_block), root_dir_inode));
        
        if (clear_dir_entry(root_dir_inode, target) == -1) {
            return -1;
        }
        inode_delete(inumber);
        return 0;
    } else {
        if (t_inode->hard_links == 0) {
            inode_delete(inumber);
        }
        if (clear_dir_entry(root_dir_inode, target) == -1)
            return -1;
        return 0;
    }
    
    return -1;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    FILE  *fp = fopen(source_path,"r");
    if (fp == NULL)
        return -1;

    int fileHandle = tfs_open(dest_path, TFS_O_CREAT); //CREATES FILE IF DOESNT EXIST, if already exists offset = 0;
    if(fileHandle == -1)  
            return -1;  //failed to open or create new file

    char buffer[state_block_size()];
    memset(buffer, 0, sizeof(buffer));  
    size_t bRead = fread(buffer, sizeof(*buffer), sizeof(buffer), fp);
    ssize_t bWritten;

    while(bRead > 0){
        bWritten = tfs_write(fileHandle, buffer, strlen(buffer));
        if(bWritten == -1)
            return -1;
        memset(buffer, 0, sizeof(buffer));
        bRead = fread(buffer, sizeof(*buffer), sizeof(buffer), fp);
    }
    if(fclose(fp) == EOF)
        return -1;
    if(tfs_close(fileHandle) == -1)
        return -1;
    return 0;
}
