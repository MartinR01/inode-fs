#ifndef INODE_INODE_H
#define INODE_INODE_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct inode {
    int32_t nodeid;                 //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    bool isDirectory;               //soubor, nebo adresar
    int8_t references;              //počet odkazů na i-uzel, používá se pro hardlinky
    int32_t file_size;              //velikost souboru v bytech
    void *direct[5];
    void *indirect[2];
    int64_t indirect1;              // 1. nepřímý odkaz (odkaz - datové bloky)
    int64_t indirect2;              // 2. nepřímý odkaz (odkaz - odkaz - datové bloky)
};
typedef struct {
    char signature[9];              //login autora FS
    char volume_descriptor[251];    //popis vygenerovaného FS
    int32_t disk_size;              //celkova velikost VFS
    int32_t cluster_size;           //velikost clusteru
    int32_t cluster_count;          //pocet clusteru
    char *bitmap_start_address;   //adresa pocatku bitmapy datových bloků
    struct inode *inode_start_address;    //adresa pocatku  i-uzlů
    char *data_start_address;     //adresa pocatku datovych bloku
} superblock;


struct directory_item {
    int32_t inode;                   // inode odpovídající souboru
    char item_name[12];              //8+3 + /0 C/C++ ukoncovaci string znak
};

/**
 * Gets next free byte inside bitmap and sets it as used
 * @return offset from bitmap or -1 if completely full
 */
int get_free_block(superblock *fs);

/**
 * Appends link to inode at the end of folder file
 * @param fs
 * @param folder_inode folder to append new file to
 * @param name name of the new file
 * @param itemid id of the inode associated with the new file
 * @return 0 if successfull
 */
int add_file_to_dir(superblock *fs, struct inode *folder_inode, char *name, int itemid);

/**
 * Creates file representing new directory (containing onlz . and ..) in the buffer
 * @param fs
 * @param buffer buffer to create new directory to (make sure it is at least 2 * sizeof(struct directory_item))
 * @param parentid id of parent's inode
 * @param selfid id of inode for this folder
 * @return 0 if successfull
 */
int make_dir_file(superblock *fs, char *buffer, int parentid, int selfid);

/**
 * Returns inode with given id
 * @param fs
 * @param id id of the inode
 * @return inode with the id, NULL if not found
 */
struct inode *get_inode_by_id(superblock *fs, int id);

/**
 * Gets next free inode
 * Equal to running get_inode(fs, ID_ITEM_FREE)
 * @param fs
 * @return free inode or NULL if all are taken
 */
struct inode *get_free_inode(superblock *fs);

/**
 * Gets id of inode associated with searched item name
 * @param dir inode of directory to search
 * @param item item name, must be exact match
 * @return id of associated inode or -1 if not found
 */
int find_in_dir(superblock *fs, struct inode *dir, char *item);

/**
 * Gets inode corresponding to last item in path
 * @param path absolute path
 * @param fs
 * @return inode of the last item on the path or NULL if any part of the path is invalid
 */
struct inode *get_inode_by_path(superblock *fs, char *path);

/**
 * Creates empty folder on specified path.
 * @param fs
 * @param path path including name of the folder. starting from root (/) and not ending with /
 * @return 0 if successfull, -1 if error
 */
int mkdir(superblock *fs, char *path);

/**
 * Copies file to free space on disk
 * @param fs
 * @param file file to be copied
 * @return inode corresponding to the file, NULL if not enough spacce on disk
 */
struct inode *copy_file_to_free_block(superblock *fs, FILE *file);

/**
 * Copies file from src to dest directory
 * @param src source file to be copied. Cannot be direcotry
 * @param dest destination in FS. if ends with /, src will be copied into that folder.
 *      If the dest doesn't end with /, src will be copied with different name.
 * @return 0 if successfull, -1 if error.
 */
int incp(superblock *fs, char *src, char *dest);

/**
 * Copies file src from disk to dest directory. Note this function rewrites any existing file.
 * @param fs
 * @param src source file located inside FS
 * @param dest destination outside FS. If ends with /, src will be copied to this directory without changing name,
 *      otherwise provided name (and suffix) will be used.
 * @return 0 if successfull, -1 if error
 */
int outcp(superblock *fs, char *src, char *dest);

/**
 * Displays all the files inside direcotry specified by path.
 * Formatted as +DIRECTORY   -FILE
 * @param fs
 * @param path directory to list
 */
void ls(superblock *fs, char *path);

/**
 * Reads and copies file associated with inode to buffer.
 * @param fs
 * @param inode inode associated with the file
 * @param buffer where should the file be saved to
 * @param startbyte specifies offset from start of file - i.e. skips this number of bytes
 * @param bytes how many bytes should be read into the buffer. Note there is no appended \0 at the end.
 * @return 0 if successfull
 */
long load_file(superblock *fs, struct inode *inode, char *buffer, long startbyte, long bytes);

/**
 * Saves buffer to datablocks associated with given inode.
 * Necessary new datablocks are taken automatically.
 * Filesize in inode is updated.
 * @param fs
 * @param inode inode of the file
 * @param file buffer with current version to be saved
 * @param filesize size of the buffer
 * @return 0 if successfull
 */
long save_file(superblock *fs, struct inode *inode, char *file, long filesize);

#endif //INODE_INODE_H
