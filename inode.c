#include "inode.h"

// priklad - verze 2019-01
// jedná se o SIMULACI souborového systému
// první i-uzel bude odkaz na hlavní adresář (1. datový blok)
// počet přímých i nepřímých odkazů je v reálném systému větší
// adresář vždy obsahuje dvojici číslo i-uzlu - název souboru
// jde jen o příklad, vlastní datové struktury si můžete upravit
#define FOLDER_PARENT ".."
#define FOLDER_SELF "."
#define PATH_DELIM "/"
#define DEFAULT_PATH_LENGTH 100

#define ERROR_FILE_NOT_FOUND "FILE NOT FOUND (neni zdroj)\n"
#define ERROR_PATH_NOT_FOUND "PATH NOT FOUND (neexistuje cilová cesta)\n"
const int32_t ROOT_ID = 1;
const int32_t ID_ITEM_FREE = 0;
long cur_dir = 1;



u_long obtain_free_block(superblock *fs){
    char *bitmap = ((char *) fs) + fs->bitmap_start_address;
    char *end_addr = ((char *) fs) + fs->inode_start_address;
    char *original_addr = bitmap;
    long datablock_no = 0;

    while(bitmap < end_addr){
        if(*bitmap == 0){
            *bitmap = 1;
            return datablock_no;
        }
        bitmap++;
        datablock_no++;
    }
    return -1;
}

void pwd(superblock *fs){
    struct inode *dir = get_inode_by_id(fs, cur_dir);
    struct inode *parent;
    char *dir_name;
    char *temp;
    char *path = "";
//    strcpy(path, "");
//    printf("name of folder: %s\n", path);
    while(dir->nodeid != ROOT_ID){
        parent = get_inode_by_id(fs, find_in_dir_by_name(fs, dir, FOLDER_PARENT));
        dir_name = find_in_dir_by_id(fs, parent, dir->nodeid);

        if(strlen(path) > 0){
            temp = path;
            path = malloc(strlen(path) + strlen(dir_name) + 1);
            strcpy(path, dir_name);
            strcat(path, PATH_DELIM);
            strcat(path, temp);
        } else {
            path = dir_name;
        }

        dir = parent;
    }

    temp = path;
    path = malloc(strlen(path) + 1);
    strcpy(path, PATH_DELIM);
    strcat(path, temp);
    printf("%s\n", path);

    if(path != NULL) free(path);
//    if(temp != NULL) free(temp);
}


int add_file_to_dir(superblock *fs, struct inode *folder_inode, char *name, int itemid){
    if(!folder_inode->isDirectory){
        printf("Not a folder!\n");
        return -1;
    }
    char *buffer = (char *) calloc(1, folder_inode->file_size + sizeof(struct directory_item));
    load_file(fs, folder_inode, buffer, folder_inode->file_size);

    struct directory_item *folder = (struct directory_item *) buffer;
    u_long index = folder_inode->file_size / sizeof(struct directory_item);

    folder[index].inode = itemid;
    strncpy(folder[index].item_name, name, 12);
    save_file(fs, folder_inode, (char *) buffer, folder_inode->file_size + sizeof(struct directory_item));

    free(buffer);
    return 0;
}

int make_dir_file(char *buffer, int parentid, int selfid){
    struct directory_item *block = (struct directory_item *) buffer;

    block[0].inode = selfid;
    strncpy(block[0].item_name, FOLDER_SELF, 12);

    block[1].inode = parentid;
    strncpy(block[1].item_name, FOLDER_PARENT, 12);

    return 0;
}


struct inode *get_inode_by_id(superblock *fs, int id){
    struct inode *addr = ((char *) fs) + fs->inode_start_address;
    struct inode *end_addr = ((char *) fs) + fs->data_start_address;

    for(int i = 0; addr + i < end_addr; i++){
        if (addr[i].nodeid == id){
            if(addr[i].nodeid == ID_ITEM_FREE){
                addr[i].nodeid = i + 1;
            }
            return &addr[i];
        }
    }
    return NULL;
}

struct inode *get_free_inode(superblock *fs){
    return get_inode_by_id(fs, ID_ITEM_FREE);
}

int find_in_dir_by_name(superblock *fs, struct inode *dir, char *item){
    struct directory_item *items = calloc(1, dir->file_size);
    load_file(fs, dir, (char *) items, dir->file_size);
    u_long n_items = dir->file_size / sizeof(struct directory_item);

    for(u_long i = 0; i < n_items; i++){
//        printf("Comparing %s to %s (%d)\n", item, items[i].item_name, items[i].inode);
        int diff = strcmp(items[i].item_name, item);
        if(diff == 0){
            int result = items[i].inode;
            free(items);
            return result;
        }
    }
    free(items);
    return -1;
}

char *find_in_dir_by_id(superblock *fs, struct inode *dir, u_long item_id){
    struct directory_item *items = calloc(1, dir->file_size);
    load_file(fs, dir, (char *) items, dir->file_size);
    u_long n_items = dir->file_size / sizeof(struct directory_item);

    for(u_long i = 0; i < n_items; i++){
//        printf("Comparing %s to %s (%d)\n", item, items[i].item_name, items[i].inode);
        u_long diff = items[i].inode - item_id;
        if(diff == 0){
            char *result = items[i].item_name;
            free(items);
            return result;
        }
    }
    free(items);
    return NULL;
}

int cd(superblock *fs, char *path){
    struct inode *new_loc = get_inode_by_path(fs, path);
    if(new_loc == NULL){
        return -1;
    }
    cur_dir = new_loc->nodeid;
    return 0;
}

struct inode *get_inode_by_path(superblock *fs, char *path){
    char *path_cpy = (char *) calloc(1, strlen(path));
    memcpy(path_cpy, path, strlen(path));

    struct inode *cur_folder;
    if (path_cpy[0] == '/'){
        // Absolute path
        cur_folder = get_inode_by_id(fs, ROOT_ID);

    } else {
        // Relative path
        cur_folder = get_inode_by_id(fs, cur_dir);
    }
    struct inode *cur_item = cur_folder;
    char *next_item = strtok(path_cpy, PATH_DELIM);
    while(true){
        if(next_item == NULL){
            free(path_cpy);
            return cur_item;
        }
        cur_folder = cur_item;
        cur_item = get_inode_by_id(fs, find_in_dir_by_name(fs, cur_folder, next_item));
        next_item = strtok(NULL, PATH_DELIM);
    }
}

int mkdir(superblock *fs, char *path){
    // copy path to new variable
    char *parent_path = calloc(1, sizeof(char) * strlen(path));
    strcpy(parent_path, path);

    // divide into parent part and name of the new folder
    char *dir_name;
    struct inode *parent_inode;
    dir_name = strrchr(parent_path, '/');
    if(dir_name == NULL){
        // Make in current directory
        dir_name = parent_path;
        parent_inode = get_inode_by_id(fs, cur_dir);
    } else {
        // Make in other directory
        *dir_name = '\0';
        dir_name++;
        parent_inode = get_inode_by_path(fs, parent_path);
    }

    // get free inode for the direcotry and buffer for its file
    struct inode *inode = get_free_inode(fs);
    char *buffer = calloc(1, 2 * sizeof(struct directory_item));

    // create the directory file and save it to disk
    make_dir_file(buffer, parent_inode->nodeid, inode->nodeid);
    save_file(fs, inode, buffer, 2 * sizeof(struct directory_item));
    inode->isDirectory = true;

    add_file_to_dir(fs, parent_inode, dir_name, inode->nodeid);
    free(parent_path);
    free(buffer);
    return 0;
}

int copy_file_to_buffer(FILE *file, char *buffer, long size){
    long read_bytes = 0;
    int read;
    do{
        read = fgetc(file);
        if(read == EOF){
            return 0;
        }
        *buffer = (char) read;
        buffer++;
        read_bytes++;
    }while (read_bytes <= size);

    return -1;
}

int incp(superblock *fs, char *src, char *dest){
    // open file
    FILE *source_file = fopen(src, "r");
    if (source_file == NULL){
        printf("File %s not found!\n", src);
        return -1;
    }
    // get filename
    char *filename = strrchr(src, '/') + 1; // go to last /
    if(filename == NULL){
        filename = src;
    }

    // copy to dir
    if(dest[strlen(dest) -1] == '/'){
        char *dest_cpy = (char *) calloc(1, sizeof(char) * strlen(dest));
        strncpy(dest_cpy, dest, (strlen(dest) - 1)); // remove the /

        struct inode *dest_inode = get_inode_by_path(fs, dest_cpy);
        if (dest_inode == NULL){
            printf("Destination %s not found!\n", dest_cpy);
            free(dest_cpy);
            return -1;
        }
        free(dest_cpy);
        // get file size
        fseek(source_file, 0, SEEK_END);
        long size = ftell(source_file);
        rewind(source_file);
        // save file
        struct inode *file_inode = get_free_inode(fs);
        if(file_inode == NULL){
            printf("Not enought space on the disk!\n");
            return -1;
        }
        char *buffer = (char *)malloc(size);
        copy_file_to_buffer(source_file, buffer, size);
        save_file(fs, file_inode, buffer, size);
        free(buffer);

        add_file_to_dir(fs, dest_inode, filename, file_inode->nodeid);
    } else { // copy and rename
        printf("NOT IMPLEMENTED!\n");
    }
    return 0;
}

int outcp(superblock *fs, char *src, char *dest){
    struct inode *src_inode = get_inode_by_path(fs, src);
    if(src_inode == NULL){
        printf(ERROR_FILE_NOT_FOUND);
    }
    char *filename;
    //is it file or folder?
    if(dest[strlen(dest)-1] == '/'){
        // if folder, use the filename from src and rest from dest
        filename = calloc(1, strlen(dest)+strlen(strrchr(src, '/'))+1);
        if(filename == NULL){
            return -1;
        }
        strcpy(filename, dest);
        strcat(filename, strrchr(src, '/'));
    }else{
        filename = dest;
    }
    FILE *dest_file = fopen(filename, "w");
    if(filename != dest){
        free(filename);
    }
    if(dest_file == NULL){
        printf(ERROR_PATH_NOT_FOUND);
        return -1;
    }

    char *buffer = (char *)calloc(1, src_inode->file_size);
    load_file(fs, src_inode, buffer, src_inode->file_size);
    fputs(buffer, dest_file);

    fclose(dest_file);
    free(buffer);
    return 0;
}

void ls(superblock *fs, char *path){
    struct inode *folder;
    if (path == NULL){
        folder = get_inode_by_id(fs, cur_dir);
    } else {
        folder = get_inode_by_path(fs, path);
    }
    // read dir file
    char *buffer = calloc(1, folder->file_size);
    load_file(fs, folder, buffer, folder->file_size);
    u_long n_items = folder->file_size / sizeof(struct directory_item);

    struct directory_item *content = (struct directory_item *) buffer;


    for(u_long i = 0; i < n_items; i++){
        struct inode *item_inode = get_inode_by_id(fs, content[i].inode);
        char dir_or_file = item_inode->isDirectory ? '+' : '-';
        printf("%c%s\n", dir_or_file, content[i].item_name);
    }
    free(buffer);
}

long load_file(superblock *fs, struct inode *inode, char *buffer, long bytes) {
    int dir_link = 0;
    void *addr = ((char *) fs) + fs->data_start_address + (fs->cluster_size * (inode->direct[dir_link++] - 1));
    long rem_bytes = bytes;
    long to_read = ((fs->cluster_size) < rem_bytes) ? (fs->cluster_size) : rem_bytes;

    while(to_read > 0 && dir_link < 5){
        memcpy(buffer, addr, to_read);

        rem_bytes -= to_read;
        buffer += to_read;
        to_read = (fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;
        addr = ((char *) fs) + fs->data_start_address + (fs->cluster_size * (inode->direct[dir_link++] - 1));
    }

    return 0;
}

long save_file(superblock *fs, struct inode *inode, char *file, u_long filesize) {
    int dir_link = 0;
    void *addr;
    long block_no;
    long rem_bytes = filesize;
    long to_save;

    while (rem_bytes > 0 && dir_link <= 5){
        block_no = inode->direct[dir_link];
        if(block_no == 0){
            u_long index = obtain_free_block(fs);
            inode->direct[dir_link] = index + 1;
            block_no = inode->direct[dir_link];
        }

        addr = ((char *) fs) + fs->data_start_address + (fs->cluster_size * (block_no - 1));
        to_save = (fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;
        memcpy(addr, file, to_save);

        rem_bytes -= to_save;
        file += to_save;
        dir_link++;
    }

    inode->file_size = filesize;
    return 0;
}
