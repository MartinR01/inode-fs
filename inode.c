#include "inode.h"

// priklad - verze 2019-01
// jedná se o SIMULACI souborového systému
// první i-uzel bude odkaz na hlavní adresář (1. datový blok)
// počet přímých i nepřímých odkazů je v reálném systému větší
// adresář vždy obsahuje dvojici číslo i-uzlu - název souboru
// jde jen o příklad, vlastní datové struktury si můžete upravit
#define FOLDER_PARENT ".."
#define FOLDER_SELF "."

#define ERROR_FILE_NOT_FOUND "FILE NOT FOUND (neni zdroj)\n"
#define ERROR_PATH_NOT_FOUND "PATH NOT FOUND (neexistuje cilová cesta)\n"
const int32_t ROOT_ID = 1;
const int32_t ID_ITEM_FREE = 0;
int32_t ID_NEXT = 2; // id to assign to next created inode

u_long obtain_free_block(superblock *fs){
    char *bitmap = fs->bitmap_start_address;
    char *end_addr = (char *) fs->inode_start_address;
    char *original_addr = bitmap;

    while(bitmap < end_addr){
        if(*bitmap == 0){
            *bitmap = 1;
            return (bitmap - original_addr);
        }
        bitmap++;
    }
    return -1;
}


int add_file_to_dir(superblock *fs, struct inode *folder_inode, char *name, int itemid){
    if(!folder_inode->isDirectory){
        printf("Not a folder!\n");
        return -1;
    }
    char *buffer = (char *) calloc(1, folder_inode->file_size + sizeof(struct directory_item));
    load_file(fs, folder_inode, buffer, 0, folder_inode->file_size);

    struct directory_item *folder = (struct directory_item *) buffer;
    u_long index = folder_inode->file_size / sizeof(struct directory_item);

    folder[index].inode = itemid;
    strncpy(folder[index].item_name, name, 12);
    save_file(fs, folder_inode, (char *) buffer, folder_inode->file_size + sizeof(struct directory_item));

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
    struct inode *addr = fs->inode_start_address;
    struct inode *end_addr = (struct inode *) fs->data_start_address;

    for(int i = 0; addr + i < end_addr; i++){
        if (addr[i].nodeid == id){
            if(addr[i].nodeid == ID_ITEM_FREE){
                addr[i].nodeid = ID_NEXT;
                ID_NEXT++;
            }
            return &addr[i];
        }
    }
    return NULL;
}

struct inode *get_free_inode(superblock *fs){
    return get_inode_by_id(fs, ID_ITEM_FREE);
}

int find_in_dir(superblock *fs, struct inode *dir, char *item){
    struct directory_item *items = calloc(1, dir->file_size);
    load_file(fs, dir, (char *) items, 0, dir->file_size);
    u_long n_items = dir->file_size / sizeof(struct directory_item);

    for(u_long i = 0; i < n_items; i++){
        int diff = strcmp(items[i].item_name, item);
        if(diff == 0){
            free(items);
            return items[i].inode;
        }
    }
    free(items);
    return -1;
}

struct inode *get_inode_by_path(superblock *fs, char *path){
    struct inode *cur_dir = get_inode_by_id(fs, ROOT_ID);
    char *searched = calloc(1, sizeof(char) * strlen(path));
    strcpy(searched, path);

    char *rest;

    searched = strchr(searched, '/');
    if(searched == NULL){  // last item on path
        return cur_dir;
    }
    *searched = '\0'; // delete /
    searched++;

    while(cur_dir->isDirectory){
        rest = strchr(searched, '/');
        if(rest == NULL){
            return get_inode_by_id(fs, find_in_dir(fs, cur_dir, searched));
        }

        *rest = '\0'; // delete /
        rest++;

        cur_dir = get_inode_by_id(fs, find_in_dir(fs, cur_dir, searched));
        if(cur_dir == NULL){
            printf("PATH NOT FOUND: %s", searched);
            return NULL;
        }
        searched = rest;
    }

    return cur_dir;
}

int mkdir(superblock *fs, char *path){
    // copy path to new variable
    char *parent_path = calloc(1, sizeof(char) * strlen(path));
    strcpy(parent_path, path);

    // divide into parent part and name of the new folder
    char *dir_name = strrchr(parent_path, '/');
    *dir_name = '\0';
    dir_name++;

    // get parent inode
    struct inode *parent_inode = get_inode_by_path(fs, parent_path);

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
            return -1;
        }
        // get file size
        fseek(source_file, 0, SEEK_END);
        long size = ftell(source_file);
        rewind(source_file);
        char *buffer = (char *)malloc(size);
        // save file
        struct inode *file_inode = get_free_inode(fs);
        if(file_inode == NULL){
            printf("Not enought space on the disk!\n");
            return -1;
        }
        copy_file_to_buffer(source_file, buffer, size);
        save_file(fs, file_inode, buffer, size);

        add_file_to_dir(fs, dest_inode, filename, file_inode->nodeid);
        free(dest_cpy);
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
    load_file(fs, src_inode, buffer, 0, src_inode->file_size);
    fputs(buffer, dest_file);

    fclose(dest_file);
    free(buffer);
    return 0;
}

void ls(superblock *fs, char *path){
    struct inode *folder = get_inode_by_path(fs, path);
    // read dir file
    char *buffer = calloc(1, folder->file_size);
    load_file(fs, folder, buffer, 0, folder->file_size);
    u_long n_items = folder->file_size / sizeof(struct directory_item);

    struct directory_item *content = (struct directory_item *) buffer;


    for(u_long i = 0; i < n_items; i++){
        struct inode *item_inode = get_inode_by_id(fs, content[i].inode);
        char dir_or_file = item_inode->isDirectory ? '+' : '-';
        printf("%c%s\n", dir_or_file, content[i].item_name);
    }
    free(buffer);
}

long load_file(superblock *fs, struct inode *inode, char *buffer, long startbyte, long bytes) {
    int dir_link = 0;
    void *start_addr = inode->direct[dir_link++] + startbyte;
    long rem_bytes = bytes;
    long to_read = ((fs->cluster_size - startbyte) < rem_bytes) ? (fs->cluster_size - startbyte) : rem_bytes;

    while(to_read > 0 && dir_link < 5){
        memcpy(buffer, start_addr, to_read);

        rem_bytes -= to_read;
        buffer += to_read;
        to_read = (fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;
        start_addr = inode->direct[dir_link++];
    }

    return 0;
}

long save_file(superblock *fs, struct inode *inode, char *file, u_long filesize) {
    int dir_link = 0;
    void *cur_addr;
    long rem_bytes = filesize;
    long to_save;

    while (rem_bytes > 0 && dir_link <= 5){
        cur_addr = inode->direct[dir_link];
        if(cur_addr == NULL){
            u_long index = obtain_free_block(fs);
            inode->direct[dir_link] = fs->data_start_address + (fs->cluster_size * index);
            cur_addr = inode->direct[dir_link];
        }

        to_save = (fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;
        memcpy(cur_addr, file, to_save);

        rem_bytes -= to_save;
        file += to_save;
        dir_link++;
    }

    inode->file_size = filesize;
    return 0;
}
