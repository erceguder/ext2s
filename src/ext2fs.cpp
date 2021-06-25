#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "ext2fs.h"

#define BLOCK_OFFSET(block_no) ((block_no - first_block) * block_size)
#define INODE_BLOCK_GROUP_ID(inode_no) ((inode_no - 1) / inodes_per_group)
#define INODE_TABLE_OFFSET(inode_table, inode_no) (BLOCK_OFFSET(inode_table) + inode_size * ((inode_no-1) % inodes_per_group))

#define DATA_BLOCK_GROUP_ID(data_block_no) ((data_block_no - first_block)/ blocks_per_group)
#define REFMAP_OFFSET(refmap, data_block_no) (BLOCK_OFFSET(refmap) + 4 * ((data_block_no - first_block) % blocks_per_group))

typedef struct ext2_super_block ext2_super_block;
typedef struct ext2_block_group_descriptor ext2_block_group_descriptor;
typedef struct ext2_inode ext2_inode;
typedef struct ext2_dir_entry ext2_dir_entry;
typedef unsigned char byte;

int img_fd;
int block_size;
int block_group_count;
uint32_t inodes_per_group;
uint32_t blocks_per_group;
uint32_t inode_size;
uint32_t first_block;
ext2_super_block super_block;

using namespace std;

void rw_bgd(bool read_bool, int block_group_id, ext2_block_group_descriptor *BGD){

    switch(block_size){
        case(1024):
        case(2048):

            lseek(img_fd, 2048 + sizeof(ext2_block_group_descriptor)*block_group_id, SEEK_SET);
            
            if (read_bool)
                read(img_fd, BGD, sizeof(ext2_block_group_descriptor));
            else
                write(img_fd, BGD, sizeof(ext2_block_group_descriptor));
                
            break;

        case(4096):

            lseek(img_fd, 4096 + sizeof(ext2_block_group_descriptor)*block_group_id, SEEK_SET);
            
            if (read_bool)
                read(img_fd, BGD, sizeof(ext2_block_group_descriptor));
            else
                write(img_fd, BGD, sizeof(ext2_block_group_descriptor));

            break;
    }
}

uint32_t allocate_inode(){

    ext2_block_group_descriptor bgd;
    uint32_t result = 0;
    bool done = false;

    for (int i=0; i < block_group_count; i++){  /* traverse all the block groups */

        rw_bgd(true, i, &bgd);  /* read the bgd */

        if (bgd.free_inode_count > 0){

            byte bitmap;
            short remainder = 8;

            lseek(img_fd, BLOCK_OFFSET(bgd.inode_bitmap), SEEK_SET);

            for (int j=0; j < ceil(inodes_per_group/8.); j++){   /* say 83 inodes_per_group, gonna iterate 10 times fully one time partially */

                if(j == (ceil(inodes_per_group/8.) - 1)){    /* the last byte */

                    if (inodes_per_group % 8 != 0){ /* it's partial! */
                        remainder = inodes_per_group % 8;
                    }
                }

                read(img_fd, &bitmap, sizeof(byte));

                if (bitmap != 255){ /* there are unused inodes */
                    byte mask;

                    for (int k=0; k < remainder; k++){  /* remainder is 8 except (possibly) last iteration */
                        mask = 1 << k;

                        if ((bitmap & mask) == 0){  /* kth bit of byte is zero */

                            bitmap |= mask;
                            super_block.free_inode_count--;
                            bgd.free_inode_count--;
                            /* need to reflect changes on img */

                            lseek(img_fd, -sizeof(bitmap), SEEK_CUR);
                            write(img_fd, &bitmap, sizeof(bitmap));     /* write exactly where bitmap's readed from */

                            rw_bgd(false, i, &bgd);                     /* bgd.free_inode_count-- */
                            lseek(img_fd, 1024, SEEK_SET);
                            write(img_fd, &super_block, sizeof(super_block));

                            /* return the allocated inode number */
                            result = i*inodes_per_group + j*8 + k + 1;

                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                }
            }
        }
        if (done)
            break;
    }
    return result;
}

uint32_t allocate_dblock(){

    ext2_block_group_descriptor bgd;
    uint32_t result = 0;
    bool done = false;

    for (int i=0; i < block_group_count; i++){

        rw_bgd(true, i, &bgd);

        if (bgd.free_block_count > 0){

            byte bitmap;
            short remainder = 8;

            lseek(img_fd, BLOCK_OFFSET(bgd.block_bitmap), SEEK_SET);

            for (int j=0; j < ceil(blocks_per_group/8.); j++){

                if (j == (ceil(blocks_per_group/8.) - 1)){
                    if (blocks_per_group % 8 != 0){     /* partial */
                        remainder = blocks_per_group % 8;
                    }
                }

                read(img_fd, &bitmap, sizeof(byte));

                if (bitmap != 255){ /* there are unused blocks */
                    byte mask;

                    for (int k=0; k < remainder; k++){  /* remainder is 8 except (possibly) last iteration */
                        mask = 1 << k;

                        if ((bitmap & mask) == 0){  /* kth bit of byte is zero */

                            bitmap |= mask;
                            super_block.free_block_count--;
                            bgd.free_block_count--;
                            /* need to reflect changes on img */

                            lseek(img_fd, -sizeof(bitmap), SEEK_CUR);
                            write(img_fd, &bitmap, sizeof(bitmap));     /* write exactly where bitmap's readed from */

                            rw_bgd(false, i, &bgd);                     /* bgd.free_inode_count-- */
                            lseek(img_fd, 1024, SEEK_SET);
                            write(img_fd, &super_block, sizeof(super_block));

                            /* return the allocated block number */
                            result = i*blocks_per_group + j*8 + k + first_block;

                            /* NEED TO UPDATE REFMAP HERE */
                            uint32_t count;
                            lseek(img_fd, REFMAP_OFFSET(bgd.block_refmap, result), SEEK_SET);
                            read(img_fd, &count, sizeof(uint32_t));

                            count++;

                            lseek(img_fd, -sizeof(uint32_t), SEEK_CUR);
                            write(img_fd, &count, sizeof(uint32_t));

                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                }
            }
            if (done)
                break;
        }
    }
    return result;
}

void copy_inode(ext2_inode* old, ext2_inode* fresh){

    *fresh = *old;

    fresh->access_time = time(0);
    fresh->creation_time = time(0);
    fresh->modification_time = time(0);
    fresh->link_count = 1;
}

uint32_t conv_abs_path_inode(char abs_path[]){
    vector<string> names;
    ext2_block_group_descriptor bgd;
    ext2_inode inode;
    ext2_dir_entry *tmp = (ext2_dir_entry*) malloc(sizeof(ext2_dir_entry) + sizeof(char[EXT2_MAX_NAME_LENGTH]));

    char * name = strtok(abs_path, "/");
    short names_index = 0;
    uint32_t inode_no = 2;
    bool break_loop = false;

    while (name != NULL){
        names.push_back(name);
        name = strtok(NULL, "/");
    }

    while(true){
        rw_bgd(true, INODE_BLOCK_GROUP_ID(inode_no), &bgd);

        lseek(img_fd, INODE_TABLE_OFFSET(bgd.inode_table, inode_no), SEEK_SET);
        read(img_fd, &inode, sizeof(ext2_inode));

        for (int i=0; i < EXT2_NUM_DIRECT_BLOCKS; i++){

            if (inode.direct_blocks[i]){
                off_t offset = 0;

                while (offset < block_size){
                    lseek(img_fd, BLOCK_OFFSET(inode.direct_blocks[i]) + offset, SEEK_SET);
                    read(img_fd, tmp, sizeof(ext2_dir_entry) + sizeof(char[EXT2_MAX_NAME_LENGTH]));

                    //cout << "tmp->name: " << tmp->name << ", length: " << tmp->length << endl;

                    if (tmp->name == names.front()){
                        names.erase(names.begin());

                        //cout << "found " << tmp->name << endl;

                        inode_no = tmp->inode;

                        break_loop = true;
                        break;
                    }

                    offset += tmp->length;
                }
            }
            if (break_loop)
                break;
        }
        if (names.size() == 0)
            break;
    }
    free(tmp);

    return inode_no;
}

int main(int argc, char* argv[]){
    img_fd = open(argv[2], O_RDWR);
    string dup("dup");
	string rm("rm");

    if (img_fd == -1){
        fprintf(stderr, "img file couldn't be found.\n");
        exit(1);
    }
    /* reading super block */
    lseek(img_fd, EXT2_SUPER_BLOCK_POSITION, SEEK_SET);
    read(img_fd, &super_block, sizeof(ext2_super_block));

    if(super_block.magic != EXT2_SUPER_MAGIC || super_block.minor_rev_level != EXT2S_MINOR_LEVEL){
        fprintf(stderr, "given FS is not ext2s.\n");
        exit(1);
    }
    /* extracting some useful info from super block */
    block_size = EXT2_UNLOG(super_block.log_block_size);
    inodes_per_group = super_block.inodes_per_group;
    blocks_per_group = super_block.blocks_per_group;
    inode_size = super_block.inode_size;
    first_block = super_block.first_data_block;

    block_group_count = ceil(((double)super_block.block_count)/blocks_per_group);

	if (dup.compare(argv[1]) == 0){ /* dup */

		if (argv[3][0] == '/'){ /* SOURCE: Absolute path */
            cout << "inode: " << conv_abs_path_inode(argv[3]) << endl;
		}
		else{   /* SOURCE: inode_no */
            uint32_t old_inode_no = atoi(argv[3]);
            uint32_t new_inode_no;

            ext2_block_group_descriptor old_inode_bgd, new_inode_bgd;
            ext2_inode old_inode, new_inode;

            if ((new_inode_no = allocate_inode()) == 0){
                /* NOTICE: super block free inode count, bgd free inode count, bgd inode bitmap are updated in allocate_inode() */
                fprintf(stderr, "no free inode left on FS.\n");
                exit(1);
            }
            else
                cout << new_inode_no << endl;

            /* read inode of given inode number */
            rw_bgd(true, INODE_BLOCK_GROUP_ID(old_inode_no), &old_inode_bgd);

            lseek(img_fd, INODE_TABLE_OFFSET(old_inode_bgd.inode_table, old_inode_no), SEEK_SET);
            read(img_fd, &old_inode, sizeof(ext2_inode));

            /* copy most of the info. from given inode to newly alloc'd inode */
            copy_inode(&old_inode, &new_inode);

            /* read BGD of newly alloc'd inode */
            rw_bgd(true, INODE_BLOCK_GROUP_ID(new_inode_no), &new_inode_bgd);
            
            /* write updated new inode to inode table of related group */
            lseek(img_fd, INODE_TABLE_OFFSET(new_inode_bgd.inode_table, new_inode_no), SEEK_SET);
            write(img_fd, &new_inode, sizeof(ext2_inode));

            /* Increment reference counts for the file blocks. */
            for (int i=0; i < EXT2_NUM_DIRECT_BLOCKS; i++){
                
                if (old_inode.direct_blocks[i] != 0){   /* if block is used */
                    
                    ext2_block_group_descriptor tmp_bgd;
                    uint32_t count = 0;
                    
                    /* read BGD of data block which old inode points to */
                    rw_bgd(true, DATA_BLOCK_GROUP_ID(old_inode.direct_blocks[i]), &tmp_bgd);

                    /* read reference count of the data block */
                    lseek(img_fd, REFMAP_OFFSET(tmp_bgd.block_refmap, old_inode.direct_blocks[i]), SEEK_SET);
                    read(img_fd, &count, sizeof(uint32_t));

                    count++;    /* OVERFLOW? */
                    /* write back the reference count */
                    lseek(img_fd, -sizeof(uint32_t), SEEK_CUR);
                    write(img_fd, &count, sizeof(uint32_t));
                }
            }

            if (argv[4][0] == '/'){ /* DEST: Absolute path */

            }
            else{   /* DEST: dir_inode/target_name */
                uint32_t dir_inode_no = atoi(strtok(argv[4], "/"));
                string target_name = strtok(NULL, "/");
                bool inserted = false;

                ext2_block_group_descriptor dir_inode_bgd;
                ext2_inode dir_inode;
                ext2_dir_entry *tmp_dir_entry, *dir_entry;
                
                tmp_dir_entry = (ext2_dir_entry*) malloc(sizeof(ext2_dir_entry) + sizeof(char[EXT2_MAX_NAME_LENGTH]));
                dir_entry = (ext2_dir_entry*) malloc(sizeof(ext2_dir_entry) + sizeof(char[target_name.length()]));

                /* creating dir entry to-be-inserted */
                dir_entry->inode = new_inode_no;
                dir_entry->name_length = target_name.length();
                dir_entry->file_type = 1;    /* regular file */
                dir_entry->length = 8 + target_name.length();

                while(dir_entry->length % 4) dir_entry->length++;

                for (int i=0; i < target_name.length(); i++)
                    dir_entry->name[i] = target_name[i];
                
                /* read BGD and then inode of directory inode number */
                rw_bgd(true, INODE_BLOCK_GROUP_ID(dir_inode_no), &dir_inode_bgd);

                lseek(img_fd, INODE_TABLE_OFFSET(dir_inode_bgd.inode_table, dir_inode_no), SEEK_SET);
                read(img_fd, &dir_inode, sizeof(ext2_inode));

                for (int i=0; i < EXT2_NUM_DIRECT_BLOCKS; i++){ /* traversing all direct blocks */

                    if (dir_inode.direct_blocks[i] != 0){   /* if block is used */

                        off_t offset = 0;
                        uint32_t expected_size;

                        while(offset < block_size){

                            lseek(img_fd, dir_inode.direct_blocks[i]*block_size + offset, SEEK_SET);
                            read(img_fd, tmp_dir_entry, sizeof(ext2_dir_entry) + sizeof(char[EXT2_MAX_NAME_LENGTH]));

                            expected_size = sizeof(ext2_dir_entry) + tmp_dir_entry->name_length;
                            while(expected_size % 4) expected_size++;

                            // cout << "name: " << +tmp_dir_entry->name << ", ";
                            // cout << "length: " << tmp_dir_entry->length << ", ";
                            // cout << "expected_size: " << expected_size << ", ";
                            // cout << "inode: " << tmp_dir_entry->inode << endl;

                            if (tmp_dir_entry->length >= dir_entry->length + expected_size){

                                // cout << "is THE ONE\n";
                                
                                dir_entry->length = tmp_dir_entry->length - expected_size;
                                tmp_dir_entry->length = expected_size;
                                
                                lseek(img_fd, dir_inode.direct_blocks[i]*block_size + offset, SEEK_SET);  /* roll back to where tmp_dir_entry was readed */
                                write(img_fd, tmp_dir_entry, sizeof(ext2_dir_entry) + sizeof(char[tmp_dir_entry->name_length]));
                                
                                lseek(img_fd, dir_inode.direct_blocks[i]*block_size + offset + expected_size, SEEK_SET);
                                write(img_fd, dir_entry, sizeof(ext2_dir_entry) + sizeof(char[dir_entry->name_length]));

                                inserted = true;
                                break;
                            }
                            else if (tmp_dir_entry->inode == 0 && tmp_dir_entry->length >= dir_entry->length){
                                /* ??? */
                            }

                            offset += tmp_dir_entry->length;
                        }
                        cout << "-1" << endl;
                        if (inserted) break;
                    }
                    else{   /* time to allocate a new data block for the directory inode */
                        uint32_t allocated_dblock = allocate_dblock();
                        ext2_dir_entry zero_entry = {0, block_size, 0, 0};

                        if (allocated_dblock == 0){
                            fprintf(stderr, "error when trying to allocate new blocks for directory.\n");
                            exit(1);
                        }
                        else
                            cout << allocated_dblock << endl;

                        dir_inode.direct_blocks[i] = allocated_dblock;

                        lseek(img_fd, allocated_dblock * block_size, SEEK_SET);
                        write(img_fd, &zero_entry, sizeof(ext2_dir_entry));

                        break;
                    }
                }
                free(tmp_dir_entry);
                free(dir_entry);
            }
		}
        super_block.write_time = time(0);
        lseek(img_fd, EXT2_SUPER_BLOCK_SIZE, SEEK_SET);
        write(img_fd, &super_block, sizeof(super_block));
	}
    close(img_fd);
}
