#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "inode.h"

#define SIZE_INPUT 100
#define ARG_DELIM " "

#define NEXT_ARG strtok(NULL, ARG_DELIM)
#define EQUAL_S(a,b) strcmp(a, b) == 0

void process_cmd(char *cmd, superblock *fs){
    // remove newline
    cmd[strlen(cmd) - 1] = '\0';

    strtok(cmd, ARG_DELIM);

    if(EQUAL_S(cmd, "ls")){
        ls(fs, NEXT_ARG);
    } else if (EQUAL_S(cmd, "mkdir")){
        mkdir(fs, NEXT_ARG);
    } else if (EQUAL_S(cmd,"incp")){
        char *arg1 = NEXT_ARG;
        char *arg2 = NEXT_ARG;
        incp(fs, arg1, arg2);  // C vykonava vlozene fce odzadu
    } else if (EQUAL_S(cmd, "outcp")){
        char *arg1 = NEXT_ARG;
        char *arg2 = NEXT_ARG;
        outcp(fs, arg1, arg2);
    } else if (EQUAL_S(cmd, "cd")){
        cd(fs, NEXT_ARG);
    } else if (EQUAL_S(cmd, "pwd")){
        pwd(fs);
    }
}


int main(int argc, const char* argv[]) {
    if(argc != 2){
        printf("Invalid number of arguments!\nUsage: inode <FS name>\n");
        exit(1);
    }

    long disk_size;
    superblock *fs;

    FILE *file = fopen(argv[1], "r");
    if (file == NULL){
//    if(1 == 1){
        printf("File %s not found... Creating a new FS...\n", argv[1]);

        // DEFAUTLS
        disk_size = 800 * 1024; // 800 kb
        int cluster_size = 1024; //b

        fs = (superblock *) calloc(1, disk_size);
        int n_datablocks = (disk_size - sizeof(superblock)) / (cluster_size + sizeof(struct inode) + 1);

        // init superblock
        fs->disk_size = disk_size;
        fs->cluster_size = cluster_size;
        fs->cluster_count = n_datablocks;
        fs->bitmap_start_address = sizeof(superblock);
        fs->inode_start_address = fs->bitmap_start_address + fs->cluster_count;
        fs->data_start_address = fs->inode_start_address + sizeof(struct inode) * fs->cluster_count;

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

        free(buffer);
    } else {
        printf("Loading data from FS %s...\n", argv[1]);

        fseek(file, 0, SEEK_END);
        disk_size = ftell(file);
        rewind(file);

        fs = (superblock *) calloc(1, disk_size);
        if(fread(fs, disk_size, 1, file) != 1 ){
            printf("Error reading file!!!!\n");
        }

        fclose(file);
    }

//    mkdir(fs, "/dev");
//    mkdir(fs, "/dev/etc");
//    incp(fs, "/home/martin/small.txt", "/dev/");
//    ls(fs, "/dev");
//    outcp(fs, "/dev/small.txt", "/home/martin/Documents/");
    char cmd[SIZE_INPUT];
    while(true){
        printf("~: ");
        fgets(cmd, SIZE_INPUT, stdin);
//        printf("'%s'\n", cmd);
        if(EQUAL_S(cmd, "q\n")){
            break;
        } else if(EQUAL_S(cmd, "w\n")){
            FILE *wfile = fopen(argv[1], "w");
            fwrite(fs, disk_size, 1, wfile);
            fclose(wfile);
        }
        process_cmd(cmd, fs);
    }



    free(fs);
    return 0;
}