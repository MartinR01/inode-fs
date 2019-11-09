#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "inode.h"


int main(int argc, const char* argv[]) {
    if(argc != 2){
        printf("Invalid number of arguments!\nUsage: inode <FS name>\n");
        exit(1);
    }

    FILE *file = fopen(argv[1], "r+");
    if (file == NULL){
        printf("File %s not found... Creating a new FS...\n", argv[1]);
    } else {
        printf("Loading data from FS %s...\n", argv[1]);
    }
    // DEFAUTLS
    int disk_size = 800 * 1024; // 800 kb
    int cluster_size = 1024; //b

    int n_datablocks = (disk_size - sizeof(superblock)) / (cluster_size + sizeof(struct inode) + 1);

    // init superblock
    superblock *fs = (superblock *) calloc(1, disk_size);
    fs->disk_size = disk_size;
    fs->cluster_size = cluster_size;
    fs->cluster_count = n_datablocks;
    fs->bitmap_start_address = ((char *) fs) + sizeof(superblock);
    fs->inode_start_address = ((char *) fs->bitmap_start_address) + fs->cluster_count;
    fs->data_start_address = ((char *) fs->inode_start_address) + sizeof(struct inode) * fs->cluster_count;

    strcpy(fs->signature, "martin");
    strcpy(fs->volume_descriptor, "sample volume.");


    // add root folder
    struct inode *root_inode = get_free_inode(fs);
    root_inode->isDirectory = true;
    root_inode->nodeid = 1;
    char *buffer = malloc(2 * sizeof(struct directory_item));
    make_dir_file(buffer, 1, 1);
    save_file(fs, root_inode, buffer, 2 * sizeof(struct directory_item));
    root_inode->file_size =  2 * sizeof(struct directory_item);

    mkdir(fs, "/dev");
    mkdir(fs, "/dev/etc");
    incp(fs, "/home/martin/seme.txt", "/dev/");
    ls(fs, "/dev");
    outcp(fs, "/dev/seme.txt", "/home/martin/Documents/");


    free(buffer);
    return 0;
}