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


int add_file_to_dir(superblock *fs, struct inode *folder_inode, char *name, int itemid){
    if(!folder_inode->isDirectory){
        printf("Not a folder!\n");
        return -1;
    }
    char *buffer = (char *) calloc(1, folder_inode->file_size + sizeof(struct directory_item));
    load_file(fs, folder_inode, buffer, 0, folder_inode->file_size);

    printf("Loaded folder: \n");
    for(int i=0; i < folder_inode->file_size; i++){
        printf("%c", buffer[i]);
    }
    printf("\n");

    struct directory_item *folder = (struct directory_item *) buffer;
    long index = folder_inode->file_size / sizeof(struct directory_item);

    folder[index].inode = itemid;
    strncpy(folder[index].item_name, name, 12);
    printf("Modified folder: \n");
    for(int i=0; i < folder_inode->file_size + sizeof(struct directory_item); i++){
        printf("%c", buffer[i]);
    }
    printf("\n");
    save_file(fs, folder_inode, (char *) buffer, folder_inode->file_size + sizeof(struct directory_item));

//    folder_inode->file_size += sizeof(struct directory_item);

    return 0;
}

int make_dir_file(superblock *fs, char *buffer, int parentid, int selfid){
//    int block_offset = get_free_block(fs);
//    printf("block offset: %d\n", block_offset);
//    if(block_offset == -1){
//        printf("Disk is full!");
//        return NULL;
//    }
    char *block = buffer;
//    printf("block\t%p\ndata\t%p\n",block, fs->data_start_address);
    *block = parentid;
    block += 4;
    strncpy(block, PARENT_FOLDER, 12);
    block += 12;

    *block = selfid;
    block += 4;
    strncpy(block, SELF_FOLDER, 12);
    block += 12;

    printf("Obsah slozky: \n");
    for(int i=0; i < (2* sizeof(struct directory_item)); i++){
        printf("%c", buffer[i]);
    }
    printf("\n");
    return 0;
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

int find_in_dir(superblock *fs, struct inode *dir, char *item){
    printf("finding in dir %d (%d bytes) for %s\n", dir->nodeid, dir->file_size, item);
    struct directory_item *items = calloc(1, dir->file_size);
    load_file(fs, dir, (char *) items, 0, dir->file_size);
    int n_items = dir->file_size / sizeof(struct directory_item);

    char *buffer = (char *) items;
    printf("Obsah slozky: \n");
    for(int i=0; i < (3* sizeof(struct directory_item)); i++){
        printf("%c", buffer[i]);
    }
    printf("\n");

    for(int i = 0; i < n_items; i++){
        int diff = strcmp(items[i].item_name, item);
        printf("comparing to: '%s'\t diff=%d\n", items[i].item_name, diff);
        if(diff == 0){
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
    char *buffer = calloc(1, 2 * sizeof(struct directory_item));
    printf("BEFORE MAKEDIRFILE -----: \n");
    for(int i = 0; i < 2 * sizeof(struct directory_item); i++){
//        printf(".");
        printf("%c", buffer[i]);
    }
    printf("\n");
    make_dir_file(fs, buffer, parent_inode->nodeid, inode->nodeid);
    printf("AFTER MAKEDIRFILE -----: \n");
    for(int i = 0; i < 2 * sizeof(struct directory_item); i++){
//        printf(".");
        printf("%c", buffer[i]);
    }
    printf("\n");
    printf("SAVING BEFORE -----: \n");
    for(int i = 0; i < parent_inode->file_size; i++){
//        printf(".");
        printf("%c", ((char *)parent_inode->direct[0])[i]);
    }
    printf("\n");
    save_file(fs, inode, buffer, 2 * sizeof(struct directory_item));
    printf("SAVING AFTER -----: \n");
    for(int i = 0; i < parent_inode->file_size; i++){
//        printf(".");
        printf("%c", ((char *)parent_inode->direct[0])[i]);
    }
    printf("\n");
    printf("SAVING AFTER - THIS INODE-----: \n");
    for(int i = 0; i < inode->file_size; i++){
//        printf(".");
        printf("%c", ((char *)inode->direct[0])[i]);
    }
    printf("\n");

    inode->isDirectory = true;
//    inode->file_size = 2 * sizeof(struct directory_item);
    printf("BEFORE ADDING -----: \n");
    for(int i = 0; i < parent_inode->file_size; i++){
//        printf(".");
        printf("%c", ((char *)parent_inode->direct[0])[i]);
    }
    printf("\n");

    add_file_to_dir(fs, parent_inode, dir_name, inode->nodeid);

    printf("AFTER ADDING -----: \n");
    for(int i = 0; i < parent_inode->file_size; i++){
//        printf(".");
        printf("%c", ((char *)parent_inode->direct[0])[i]);
    }
    printf("\n");

    free(buffer);
    free(parent_path);
    return 0;
}

struct inode *copy_file_to_free_block(superblock *fs, FILE *file){
    struct inode *inode = get_free_inode(fs);
    int block_id = get_free_block(fs);
    char *block = fs->data_start_address + block_id;
    printf("\t block1: %p\n", block);
    inode->direct[0] = block;
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
    printf("LS -----: \n");
    for(int i = 0; i < folder->file_size; i++){
//        printf(".");
        printf("%c", ((char *)folder->direct[0])[i]);
    }
    printf("\n");

    // read dir file
    char *buffer = calloc(1, folder->file_size);
    load_file(fs, folder, buffer, 0, folder->file_size);
    long n_items = folder->file_size / sizeof(struct directory_item);

    printf("Loaded slozky (%d): \n", n_items);
    for(int i = 0; i < n_items * sizeof(struct directory_item); i++){
//        printf(".");
        printf("%d ", buffer[i]);
    }
    printf("\n");
    struct directory_item *content = (struct directory_item *) buffer;


    for(long i = 0; i < n_items; i++){
        printf("Item %s\tId%d\n", content[i].item_name, content[i].inode);
//        struct inode *item_inode = get_inode_by_id(fs, content[i].inode);
//        char dir_or_file = item_inode->isDirectory ? '+' : '-';
//        printf("%c%s\n", dir_or_file, content[i].item_name);
    }
    free(buffer);
}

long load_file(superblock *fs, struct inode *inode, char *buffer, long startbyte, long bytes) {
    void *start_addr = inode->direct[0] + startbyte;
    printf("On disk: \n");
    for(int i = 0; i < bytes; i++){
//        printf(".");
        printf("%c", ((char *)inode->direct[0])[i]);
    }
    printf("\n");
    long rem_bytes = bytes;
    long to_read = ((fs->cluster_size - startbyte) < rem_bytes) ? (fs->cluster_size - startbyte) : rem_bytes;

    printf("copying %d bytes...\n", to_read);
    memcpy(buffer, start_addr, to_read);


    printf("Loaded: \n");
    for(int i = 0; i < to_read; i++){
//        printf(".");
        printf("%c", buffer[i]);
    }
    printf("\n");

    rem_bytes -= to_read;
    buffer += to_read;
    to_read = (fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;

    return 0;
}

long save_file(superblock *fs, struct inode *inode, char *file, long filesize) {
    void *cur_addr = inode->direct[0];
    long rem_bytes = filesize;
    long to_save;

    to_save =(fs->cluster_size < rem_bytes) ? fs->cluster_size : rem_bytes;
    if(cur_addr == NULL){
        int index = get_free_block(fs);
        inode->direct[0] = fs->data_start_address + (fs->cluster_size * index);
        cur_addr = inode->direct[0];
    }
    memcpy(cur_addr, file, to_save);
    rem_bytes -= to_save;
    file += to_save;

    printf("Saved: \n");
    char *saved = cur_addr;
    for(int i = 0; i < to_save; i++){
//        printf(".");
        printf("%c", saved[i]);
    }
    printf("\n");

    inode->file_size = filesize;
    printf("Saved on disk: \n");
    for(int i = 0; i < filesize; i++){
//        printf(".");
        printf("%c", ((char *)inode->direct[0])[i]);
    }
    printf("\n");
    return 0;
}
