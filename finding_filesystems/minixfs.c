/**
 * Finding Filesystems
 * CS 241 - Spring 2020
 */
#include "minixfs.h"
#include "minixfs_utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/**
 * Virtual paths:
 *  Add your new virtual endpoint to minixfs_virtual_path_names
 */
char *minixfs_virtual_path_names[] = {"info", /* add your paths here*/};

/**
 * Forward declaring block_info_string so that we can attach unused on it
 * This prevents a compiler warning if you haven't used it yet.
 *
 * This function generates the info string that the virtual endpoint info should
 * emit when read
 */
static char *block_info_string(ssize_t num_used_blocks) __attribute__((unused));
static char *block_info_string(ssize_t num_used_blocks) {
    char *block_string = NULL;
    ssize_t curr_free_blocks = DATA_NUMBER - num_used_blocks;
    asprintf(&block_string, "Free blocks: %zd\n"
                            "Used blocks: %zd\n",
             curr_free_blocks, num_used_blocks);
    return block_string;
}

// Don't modify this line unless you know what you're doing
int minixfs_virtual_path_count =
    sizeof(minixfs_virtual_path_names) / sizeof(minixfs_virtual_path_names[0]);

int minixfs_chmod(file_system *fs, char *path, int new_permissions) {
    struct stat buffer;
    int success = minixfs_stat(fs, path, &buffer);
    if (success == -1) {
        errno = ENOENT;
        return -1;
    }

   inode* curr_inode = get_inode(fs, path); // 2
   if (curr_inode == NULL) {
       errno = ENOENT;
       return -1;
   }
   
   int mode = curr_inode->mode >> RWX_BITS_NUMBER;
   curr_inode->mode = (new_permissions | (mode << RWX_BITS_NUMBER));

   clock_gettime(CLOCK_REALTIME, &(curr_inode->ctim)); // 4
   
   return 0; // 5
}

int minixfs_chown(file_system *fs, char *path, uid_t owner, gid_t group) {
    
    inode* curr_inode = get_inode(fs, path);
    if (curr_inode == NULL) {
        errno = ENOENT;
        return -1;
    }
    
    if (owner != (uid_t)-1) {
        curr_inode->uid = owner;
    }
    if (group != (gid_t)-1) {
        curr_inode->gid = group;
    }

    clock_gettime(CLOCK_REALTIME, &(curr_inode->ctim));

    return 0;
}

inode *minixfs_create_inode_for_path(file_system *fs, const char *path) {

    inode* curr_inode = get_inode(fs, path); // inode exists
    inode_number curr_inode_num = first_unused_inode(fs); // inode be created
    
    if (curr_inode != NULL || curr_inode_num == -1) {
        return NULL; 
    }

    const char* const_filename = NULL;
    inode* parent = parent_directory(fs, path, &const_filename);

    char* curr_filename = (char*)const_filename;    
    int valid_file = valid_filename(curr_filename); // filename is valid file
    if (valid_file != 1) {
        return NULL;
    }

    // traverse through inode list to find new inode
    inode* inode_root = fs->inode_root;
    uint64_t num_inodes = fs->meta->inode_count;
    uint64_t i = 0;
    inode* new_inode = inode_root;
    while (i < num_inodes) {
        if (inode_root[i].nlink == 0) { // it's unused
            new_inode = inode_root+i;
            break;
        }
        i++;
    }

    minixfs_dirent* source = malloc(sizeof(minixfs_dirent));
    source->name = curr_filename;
    source->inode_num = curr_inode_num;

    char block[FILE_NAME_ENTRY];
    make_string_from_dirent(block, *source);

    // create parent path:
    char* parent_path = malloc(strlen(path));
    strcpy(parent_path, path);

    int path_len = (size_t)strlen(parent_path);

    for (int i = path_len - 1; i >= 0; i--) {
        if (parent_path[i] == '/') {
            parent_path[i] = 0;
            break;
        }
    }

    off_t offset = FILE_NAME_ENTRY;
    minixfs_write(fs, parent_path, block, FILE_NAME_ENTRY, &offset);
    
    init_inode(parent, new_inode);
    return new_inode; // need to return the new inode
}

ssize_t minixfs_virtual_read(file_system *fs, const char *path, void *buf,
                             size_t count, off_t *off) {
    if (!strcmp(path, "info")) {
        // TODO implement the "info" virtual file here
        int used = 0;
        for (int i = 0; i < DATA_NUMBER; i++) {
            if (get_data_used(fs, i)) {
                used++;
            }
        }

        char* file_contents = block_info_string((ssize_t)used);
        // write file_contents into buf
        size_t len = strlen(file_contents) - *off;
        size_t min = 0;
        if (count < len) {
            min = count;
        } else {
            min = len;
        }

        memcpy(buf, file_contents + *off, min);
        *off += min;
        return min;
    }
    // TODO implement your own virtual file here
    errno = ENOENT;
    return -1;
}

ssize_t minixfs_write(file_system *fs, const char *path, const void *buf,
                      size_t count, off_t *off) {
    // X marks the spot
    //fprintf(stderr, "ENTERING WRITE FUNCTION \n");
    size_t max_file_size = ((NUM_DIRECT_BLOCKS) * (16 * KILOBYTE)) + ((NUM_INDIRECT_BLOCKS) * (16 * KILOBYTE));
    if (*off + count > max_file_size) { // write request more than possible file size
        errno = ENOSPC;
        return (ssize_t)-1;
    }

    inode* curr = get_inode(fs, path);
    if (curr == NULL) { // file doesn't already exist
        //fprintf(stderr, "file doesn't already exist \n");
        curr = minixfs_create_inode_for_path(fs, path);
        if (curr == NULL) {
            //fprintf(stderr, "failed to create inode \n");
            errno = ENOSPC;
            return (ssize_t)-1;
        }
    }

    // num blocks to allocate
    int blocks_needed = (int)ceil((double)(*off + count / sizeof(data_block)));

    // min_blockcount to allocate blocks 
    int s = minixfs_min_blockcount(fs, path, blocks_needed);
    if (s == -1) {
        errno = ENOSPC;
        return (ssize_t)-1;
    }

    data_block_number* direct = curr->direct;
    data_block* data_root = fs->data_root;

    unsigned long left_index = *off / (16 * KILOBYTE); // starting block
    unsigned long right_index = *off - (left_index * (16*KILOBYTE)); // see if ends in direct block

    size_t bytes_to_write = count;

    // CASE 1: offset is within a single direct data block
    if (left_index < NUM_DIRECT_BLOCKS && count + right_index <= (16*KILOBYTE)) { 
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        memcpy(data + right_index, buf, count);
    }
    
    // CASE 2: offset starts in single direct block and leaks into other direct and indirect blocks
    else if (left_index < NUM_DIRECT_BLOCKS && count + right_index > (16*KILOBYTE)) {
        unsigned long leftover = (16*KILOBYTE) - right_index;

        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        memcpy(data + right_index, buf, leftover);
        bytes_to_write -= leftover;
        buf += leftover;
        left_index += 1;

        while (left_index < NUM_DIRECT_BLOCKS) { // need to write into neighboring direct blocks
            data_block_number other_direct_num = direct[left_index];
            data_block* other_direct_block = data_root + other_direct_num;
            char* other_data = other_direct_block->data;
            
            size_t min = 0;
            if (bytes_to_write < (16*KILOBYTE)) {
                min = bytes_to_write;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(other_data, buf, min);
            buf += min;
            bytes_to_write -= min;
            left_index++;

            if (bytes_to_write <= 0) {
                break;
            }
        }
        
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        while (bytes_to_write > 0) { // need to write into indirect blocks 
            data_block_number indirect_num = *indirect_block_array;
            data_block* indirect_block = data_root + indirect_num;
            char* data = indirect_block->data;

            size_t min = 0;
            if (bytes_to_write < (16*KILOBYTE)) {
                min = bytes_to_write;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(data, buf, min);
            buf += min;
            bytes_to_write -= min;
            indirect_block_array++;
        }
        // now bytes_to_write == 0
       
    }

    // CASE 3: offset is in single indirect block only
    else if (left_index > NUM_DIRECT_BLOCKS && count + right_index <= (16 * KILOBYTE)) {
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        unsigned long indirect_offset = left_index - NUM_DIRECT_BLOCKS;
        data_block_number* indirect_block_num = indirect_block_array + indirect_offset;
        data_block* indirect_block = data_root + *indirect_block_num;
        char* data = indirect_block->data;

        memcpy(data + right_index, buf, count);
        
    }

    // CASE 4: offset starts in indirect block and leaks to other indirect blocks
    else if (left_index > NUM_DIRECT_BLOCKS && count + right_index > (16 * KILOBYTE)) {
        unsigned long leftover = (16 * KILOBYTE) - right_index;

        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        unsigned long indirect_offset = left_index - NUM_DIRECT_BLOCKS;
        data_block_number* indirect_block_num = indirect_block_array + indirect_offset;
        data_block* indirect_block = data_root + *indirect_block_num;
        char* data = indirect_block->data;

        memcpy(data + right_index, buf, leftover);
        bytes_to_write -= leftover;
        buf += leftover;
        indirect_block_num++;

        while (bytes_to_write > 0) { // need to write into neighboring indirect blocks
            data_block* other_indirect_block = data_root + *indirect_block_num;
            char* other_data = other_indirect_block->data;

            size_t min = 0;
            if (bytes_to_write < (16*KILOBYTE)) {
                min = bytes_to_write;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(other_data, buf, min);
            buf += min;
            bytes_to_write -= min;
            indirect_block_num++;
        }
        // done writing all bytes
    }
    curr->size = *off + count; // the constant stuff
    *off += count;
    clock_gettime(CLOCK_REALTIME, &(curr->atim));
    clock_gettime(CLOCK_REALTIME, &(curr->mtim));

    return count;


    // Case 1: all in direct blocks
    /*if (right_index < NUM_DIRECT_BLOCKS) {
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        // write buf to data:
        fprintf(stderr, "before memcpy, #1 --%s-- \n", data);
        memcpy(data + *off, buf, count);
        fprintf(stderr, "after memcpy, #1 --%s-- \n", data);
        *off += count;
    }
    // Case 2: some in direct, some in indirect
    else if (left_index > NUM_DIRECT_BLOCKS && right_index < NUM_INDIRECT_BLOCKS) {
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        // write part of buf to data:
        fprintf(stderr, "before memcpy, #2a --%s-- \n", data);
        size_t bytes_to_direct = *off - (NUM_DIRECT_BLOCKS * sizeof(data_block));
        memcpy(data + *off, buf, bytes_to_direct);
        fprintf(stderr, "after memcpy, #2a --%s-- \n", data);
        *off += bytes_to_direct;

        off_t index1 = left_index - NUM_DIRECT_BLOCKS;
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block = (data_block_number*)(data_root+indirect_num);
        
        data_block_number a = indirect_block[index1];
        data_block* b = data_root + a;
        char* data1 = b->data;

        // write buf to data:
        fprintf(stderr, "before memcpy, #2b --%s-- \n", data1);
        memcpy(data1 + *off, buf, count - bytes_to_direct);
        fprintf(stderr, "after memcpy, #2b --%s-- \n", data1);
        *off += (count - bytes_to_direct);
    }
    // Case 3: all in indirect blocks
    else if (left_index > NUM_DIRECT_BLOCKS) {
        off_t index1 = left_index - NUM_DIRECT_BLOCKS;
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block = (data_block_number*)(data_root+indirect_num);
        
        data_block_number a = indirect_block[index1];
        data_block* b = data_root + a;
        char* data = b->data;

        // write buf to data:
        fprintf(stderr, "before memcpy, #3 --%s-- \n", data);
        memcpy(data + *off, buf, count);
        fprintf(stderr, "after memcpy, #3 --%s-- \n", data);
        *off += count;
    }

    // modify atim and mtim
    clock_gettime(CLOCK_REALTIME, &(curr->atim));
    clock_gettime(CLOCK_REALTIME, &(curr->mtim));
    return count;*/
}

ssize_t minixfs_read(file_system *fs, const char *path, void *buf, size_t count,
                     off_t *off) {
    const char *virtual_path = is_virtual_path(path);
    if (virtual_path)
        return minixfs_virtual_read(fs, virtual_path, buf, count, off);
    // 'ere be treasure!

    // divide offset by 16 KB to get the direct[off / 16KB]
    // data_block_num from that data_root[] 
    // that char array is what we need to write into buffer

    // need to check if off / 16 KB is greater than or equal to 11
    // if its greater then its inside a indirect block

    inode* curr = get_inode(fs, path);
    //fprintf(stderr, "ENTERING READ FUNCTION \n");
    if (curr == NULL) { // if path doesn't exist
        //fprintf(stderr, "read ( ) : inode is null \n");
        errno = ENOENT; 
        return -1;
    }

    // if offset > file size: 
    if (*(uint64_t*)off >= curr->size) return 0;
    
    data_block_number* direct = curr->direct;
    data_block* data_root = fs->data_root;

    unsigned long left_index = *off / (16 * KILOBYTE); // starting block
    unsigned long right_index = *off - (left_index * (16*KILOBYTE)); // see if ends in direct block
    size_t bytes_read = 0;

    size_t bytes_to_read = (count < curr->size) ? count : (curr->size-*off);

    // CASE 1: offset is within a single direct data block
    if (left_index < NUM_DIRECT_BLOCKS && bytes_to_read + right_index <= (16*KILOBYTE)) { 
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        memcpy(buf, data + right_index, bytes_to_read);
        bytes_read += bytes_to_read;
        *off += bytes_read;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        /*curr->size = *off + count; // the constant stuff
        *off += count;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        clock_gettime(CLOCK_REALTIME, &(curr->mtim));*/
        return bytes_read;
    }

    // CASE 2: offset is in single indirect block only
    else if (left_index > NUM_DIRECT_BLOCKS && bytes_to_read + right_index <= (16 * KILOBYTE)) {
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        unsigned long indirect_offset = left_index - NUM_DIRECT_BLOCKS;
        data_block_number* indirect_block_num = indirect_block_array + indirect_offset;
        data_block* indirect_block = data_root + *indirect_block_num;
        char* data = indirect_block->data;

        memcpy(buf, data + right_index, bytes_to_read);
        //curr->size = *off + count; // the constant stuff
        *off += bytes_to_read;
        // set times
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return bytes_to_read;
    }
    
    // CASE 3: offset starts in single direct block and leaks into other direct and indirect blocks
    else if (left_index < NUM_DIRECT_BLOCKS && bytes_to_read + right_index > (16*KILOBYTE)) {
        unsigned long leftover = (16*KILOBYTE) - right_index;

        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        memcpy(buf, data + right_index, leftover);
        bytes_to_read -= leftover;
        bytes_read += leftover;
        buf += leftover;
        left_index += 1;

        while (left_index < NUM_DIRECT_BLOCKS) { // need to write into neighboring direct blocks
            data_block_number other_direct_num = direct[left_index];
            data_block* other_direct_block = data_root + other_direct_num;
            char* other_data = other_direct_block->data;
            
            size_t min = 0;
            if (bytes_to_read < (16*KILOBYTE)) {
                min = bytes_to_read;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(buf, other_data, min);
            buf += min;
            bytes_to_read -= min;
            bytes_read += min;
            left_index++;

            if (bytes_to_read <= 0) {
                /*curr->size = *off + count; // the constant stuff
                *off += count;
                // set times
                return count;*/
                *off += bytes_read;
                clock_gettime(CLOCK_REALTIME, &(curr->atim));
                return bytes_read;
            }
        }
        
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        while (bytes_to_read > 0) { // need to write into indirect blocks 
            data_block_number indirect_num = *indirect_block_array;
            data_block* indirect_block = data_root + indirect_num;
            char* data = indirect_block->data;

            size_t min = 0;
            if (bytes_to_read < (16*KILOBYTE)) {
                min = bytes_to_read;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(buf, data, min);
            buf += min;
            bytes_to_read -= min;
            bytes_read += min;
            indirect_block_array++;
        }
        // now bytes_to_write == 0
        *off += bytes_read;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return bytes_read;
        /*curr->size = *off + count; // the constant stuff
        *off += count;
        // set times
        return count;*/
    }

    // CASE 4: offset starts in indirect block and leaks to other indirect blocks
    else if (left_index > NUM_DIRECT_BLOCKS && bytes_to_read + right_index > (16 * KILOBYTE)) {
        unsigned long leftover = (16 * KILOBYTE) - right_index;

        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block_array = (data_block_number*)(data_root+indirect_num);

        unsigned long indirect_offset = left_index - NUM_DIRECT_BLOCKS;
        data_block_number* indirect_block_num = indirect_block_array + indirect_offset;
        data_block* indirect_block = data_root + *indirect_block_num;
        char* data = indirect_block->data;

        memcpy(buf, data + right_index, leftover);
        bytes_to_read -= leftover;
        bytes_read += leftover;
        buf += leftover;
        indirect_block_num++;

        while (bytes_to_read > 0) { // need to write into neighboring indirect blocks
            data_block* other_indirect_block = data_root + *indirect_block_num;
            char* other_data = other_indirect_block->data;

            size_t min = 0;
            if (bytes_to_read < (16*KILOBYTE)) {
                min = bytes_to_read;
            } else {
                min = (16 * KILOBYTE);
            }
            memcpy(buf, other_data, min);
            buf += min;
            bytes_read += min;
            bytes_to_read -= min;
            indirect_block_num++;
        }
        // done writing all bytes
       // curr->size = *off + count; // the constant stuff
        *off += bytes_read;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        // set times
        return bytes_read;
    }
    
    return count;
    
    /*unsigned long left_index = *off / (16 * KILOBYTE); // starting
    unsigned long right_index = *off + count / (16 * KILOBYTE); // end

    // Case 1: all in direct blocks
    if (right_index < NUM_DIRECT_BLOCKS) {
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        // write buf to data:
        size_t len = strlen(data) - *off;
        size_t min = 0;
        if (count < len) {
            min = count;
        } else {
            min = len;
        }

        memcpy(buf, data + *off, min);
        
        *off += min;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return min;
    }
    // Case 2: some in direct, some in indirect
    else if (left_index > NUM_DIRECT_BLOCKS && right_index < NUM_INDIRECT_BLOCKS) {
        data_block_number direct_num = direct[left_index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;

        // write buf to data:
        size_t bytes_to_direct = *off - (NUM_DIRECT_BLOCKS * sizeof(data_block));

        size_t len = strlen(data) - *off;
        size_t min = 0;
        if (bytes_to_direct < len) {
            min = bytes_to_direct;
        } else {
            min = len;
        }

        memcpy(buf, data + *off, min);
        *off += min;

        off_t index1 = left_index - NUM_DIRECT_BLOCKS;
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block = (data_block_number*)(data_root+indirect_num);
        
        data_block_number a = indirect_block[index1];
        data_block* b = data_root + a;
        char* data1 = b->data;

        // write buf to data:
        size_t len1 = strlen(data1) - *off;
        size_t min1 = 0;
        if (count - bytes_to_direct < len1) {
            min1 = count - bytes_to_direct;
        } else {
            min1 = len1;
        }
        memcpy(buf, data1 + *off, count - bytes_to_direct);
        *off += (count - bytes_to_direct);

        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return min + min1;
    }
    // Case 3: all in indirect blocks
    else if (left_index > NUM_DIRECT_BLOCKS) {
        off_t index1 = left_index - NUM_DIRECT_BLOCKS;
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block = (data_block_number*)(data_root+indirect_num);
        
        data_block_number a = indirect_block[index1];
        data_block* b = data_root + a;
        char* data = b->data;

        // write buf to data:
        size_t len = strlen(data) - *off;
        size_t min = 0;
        if (count < len) {
            min = count;
        } else {
            min = len;
        }

        memcpy(buf, data + *off, min);
        
        *off += min;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return min;
    }*/

    /*if (index > NUM_DIRECT_BLOCKS) { // indirect block
        off_t index1 = index - NUM_DIRECT_BLOCKS;
        data_block_number indirect_num = curr->indirect;
        data_block_number* indirect_block = (data_block_number*)(data_root+indirect_num);
        
        data_block_number a = indirect_block[index1];
        data_block* b = data_root + a;
        char* data = b->data;
        
        // write data into buf:
        size_t len = strlen(data) - *off;
        size_t min = 0;
        if (count < len) {
            min = count;
        } else {
            min = len;
        }

        memcpy(buf, data + *off, min);
        
        *off += min;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return min;

    } else { // direct block
        data_block_number direct_num = direct[index];
        data_block* direct_block = data_root + direct_num;
        char* data = direct_block->data;
        
        // write data into buf:
        size_t len = strlen(data) - *off;
        size_t min = 0;
        if (count < len) {
            min = count;
        } else {
            min = len;
        }

        memcpy(buf, data + *off, min);
        
        *off += min;
        clock_gettime(CLOCK_REALTIME, &(curr->atim));
        return min;
    }
    

    return -1;*/
}
