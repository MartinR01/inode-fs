#include "inode.h"

// priklad - verze 2019-01
// jedná se o SIMULACI souborového systému
// první i-uzel bude odkaz na hlavní adresář (1. datový blok)
// počet přímých i nepřímých odkazů je v reálném systému větší
// adresář vždy obsahuje dvojici číslo i-uzlu - název souboru
// jde jen o příklad, vlastní datové struktury si můžete upravit
#define PARENT_FOLDER ".."
#define SELF_FOLDER "."

#define ERROR_FILE_NOT_FOUND "FILE NOT FOUND (neni zdroj)\n"
#define ERROR_PATH_NOT_FOUND "PATH NOT FOUND (neexistuje cilová cesta)\n"
const int32_t ROOT_ID = 1;
const int32_t ID_ITEM_FREE = 0;
int32_t ID_NEXT = 2; // id to assign to next created inode

int get_free_block(superblock *fs){
    char *bitmap = fs->bitmap_start_address;
    void *end_addr = fs->inode_start_address;
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


int add_file_to_dir(struct inode *folder_inode, char *name, int itemid){
    if(!folder_inode->isDirectory){
        printf("Not a folder!\n");
        return -1;
    }

    int index = folder_inode->file_size / sizeof(struct directory_item);
    struct directory_item *folder = folder_inode->direct1;

    folder[index].inode = itemid;
    strncpy(folder[index].item_name, name, 12);

    folder_inode->file_size += sizeof(struct directory_item);

    return 0;
}

void *make_dir_file(superblock *fs, int parentid, int selfid){
    int block_offset = get_free_block(fs);
    printf("block offset: %d\n", block_offset);
    if(block_offset == -1){
        printf("Disk is full!");
        return NULL;
    }
    struct directory_item *block = fs->data_start_address + block_offset * fs->cluster_size;
    printf("block\t%p\ndata\t%p\n",block, fs->data_start_address);
    strncpy(block[0].item_name, PARENT_FOLDER, 12);
    block[0].inode = parentid;

    strncpy(block[1].item_name, SELF_FOLDER, 12);
    block[1].inode = selfid;

    return block;
}


struct inode *get_inode_by_id(superblock *fs, int id){
    struct inode *addr = fs->inode_start_address;
    void *end_addr = fs->data_start_address;

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

int find_in_dir(struct inode *dir, char *item){
    printf("finding in dir %d for %s\n", dir->nodeid, item);
    struct directory_item *items = (struct directory_item *) dir->direct1;
    int n_items = dir->file_size / sizeof(struct directory_item);

    for(int i = 0; i < n_items; i++){
        int diff = strcmp(items[i].item_name, item);
        printf("comparing to: '%s'\t diff=%d\n", items[i].item_name, diff);
        if(diff == 0){
            return items[i].inode;
        }
    }
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
            return get_inode_by_id(fs, find_in_dir(cur_dir, searched));
        }

        *rest = '\0'; // delete /
        rest++;

        cur_dir = get_inode_by_id(fs, find_in_dir(cur_dir, searched));
        if(cur_dir == NULL){
            printf("PATH NOT FOUND: %s", searched);
            return NULL;
        }
        searched = rest;
    }

    return cur_dir;
}

int mkdir(superblock *fs, char *path){
    char *parent_path = calloc(1, sizeof(char) * strlen(path));
    strcpy(parent_path, path);

    // get parent inode
    char *dir_name = strrchr(parent_path, '/');
    *dir_name = '\0';
    dir_name++;

    printf("parent\t%s\nfolder\t%s\n", parent_path, dir_name);
    struct inode *parent_inode = get_inode_by_path(fs, parent_path);
    printf("found\t%d\n", parent_inode->nodeid);

    struct inode *inode = get_free_inode(fs);
    inode->direct1 = make_dir_file(fs, parent_inode->nodeid, inode->nodeid);

    inode->isDirectory = true;
    inode->file_size = 2 * sizeof(struct directory_item);

    add_file_to_dir(parent_inode, dir_name, inode->nodeid);
    return 0;
}

struct inode *copy_file_to_free_block(superblock *fs, FILE *file){
    struct inode *inode = get_free_inode(fs);
    int block_id = get_free_block(fs);
    char *block = fs->data_start_address + block_id;
    printf("\t block1: %p\n", block);
    inode->direct1 = block;
    int max_block_size = fs->cluster_size;
    long read_bytes = 0;
    int read;

    for(;;){
        read = fgetc(file);
        if(read == EOF){
            break;
        }
        *block = read;
        block++;
        read_bytes++;
    }

    inode->file_size = read_bytes;
    printf("copied %ld bytes", read_bytes);
    return inode;
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

        printf("Puvodni cesta:\t%s\nNova cesta:\t%s\n", dest, dest_cpy);
        struct inode *dest_inode = get_inode_by_path(fs, dest_cpy);
        if (dest_inode == NULL){
            printf("Destination %s not found!\n", dest_cpy);
            return -1;
        }
        printf("Directory found:\t%d\n", dest_inode->nodeid);
        // save file
        struct inode *file_inode = copy_file_to_free_block(fs, source_file);
        if(file_inode == NULL){
            printf("Not enought space on the disk!\n");
            return -1;
        }
        add_file_to_dir(dest_inode, filename, file_inode->nodeid);
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
    long to_read = src_inode->file_size;
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

    char *data = (char *) src_inode->direct1;
    char *buffer = (char *)calloc(1, fs->cluster_size + 1);
    strncpy(buffer, data, fs->cluster_size);
    printf("to be copied:\t%s\t(%ld)\n", data, to_read);
    fputs(buffer, dest_file);

    to_read -= fs->cluster_size;
    if(to_read > 0){
        printf("Oops - the file is over one data block");
    }

    fclose(dest_file);
    return 0;
}

void ls(superblock *fs, char *path){
    struct inode *folder = get_inode_by_path(fs, path);

    // read dir file
    struct directory_item *content = (struct directory_item *) folder->direct1;
    int n_items = folder->file_size / sizeof(struct directory_item);

    for(int i = 0; i<n_items; i++){
        struct inode *item_inode = get_inode_by_id(fs, content[i].inode);
        char dir_or_file = item_inode->isDirectory ? '+' : '-';
        printf("%c%s\n", dir_or_file, content[i].item_name);
    }

//    return 0;
}

long load_file(superblock *fs, struct inode *inode, char *buffer, long startbyte, long endbyte) {
    return 0;
}
