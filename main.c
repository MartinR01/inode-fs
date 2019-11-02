#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// priklad - verze 2019-01
// jedná se o SIMULACI souborového systému
// první i-uzel bude odkaz na hlavní adresář (1. datový blok)
// počet přímých i nepřímých odkazů je v reálném systému větší
// adresář vždy obsahuje dvojici číslo i-uzlu - název souboru
// jde jen o příklad, vlastní datové struktury si můžete upravit
#define PARENT_FOLDER ".."
#define SELF_FOLDER "."

const int32_t ROOT_ID = 1;
const int32_t ID_ITEM_FREE = 0;
int32_t ID_NEXT = 2; // id to assign to next created inode


struct inode {
    int32_t nodeid;                 //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    bool isDirectory;               //soubor, nebo adresar
    int8_t references;              //počet odkazů na i-uzel, používá se pro hardlinky
    int32_t file_size;              //velikost souboru v bytech
    int64_t direct1;                // 1. přímý odkaz na datové bloky
    int64_t direct2;                // 2. přímý odkaz na datové bloky
    int64_t direct3;                // 3. přímý odkaz na datové bloky
    int64_t direct4;                // 4. přímý odkaz na datové bloky
    int64_t direct5;                // 5. přímý odkaz na datové bloky
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

/**
 * Adds a file representing folder on next free datablock
 * @param fs
 * @param parentid id of parent folder inode
 * @param selfid id of self folder inode
 * @return address of created directory or NULL if no disk is full
 */
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


struct inode *get_inode(superblock *fs, int id){
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

/**
 * Gets next free inode
 * Equal to running get_inode(fs, ID_ITEM_FREE)
 * @param fs
 * @return free inode or NULL if all are taken
 */
struct inode *get_free_inode(superblock *fs){
    return get_inode(fs, ID_ITEM_FREE);
}

/**
 * Gets id of inode associated with searched item name
 * @param dir inode of directory to search
 * @param item item name, must be exact match
 * @return id of associated inode or -1 if not found
 */
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

/**
 * Gets index of inode corresponding to last directory in path
 * !! zatim jen direct1
 * @param path path to dir starting with / (root)
 * @param fs
 * @return
 */
struct inode *get_dir_inode(superblock *fs, char *path){
    struct inode *cur_dir = get_inode(fs, ROOT_ID);
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
            return get_inode(fs, find_in_dir(cur_dir, searched));
        }

        *rest = '\0'; // delete /
        rest++;

        cur_dir = get_inode(fs, find_in_dir(cur_dir, searched));
        if(cur_dir == NULL){
            printf("PATH NOT FOUND: %s", searched);
            return NULL;
        }
        searched = rest;
    }

    return cur_dir;
}

/**
 * Creates empty folder on specified path.
 * @param fs
 * @param path path including name of the folder. starting from root (/) and not ending with /
 * @return 0 if successfull, -1 if error
 */
int mkdir(superblock *fs, char *path){
    char *parent_path = calloc(1, sizeof(char) * strlen(path));
    strcpy(parent_path, path);

    // get parent inode
    char *dir_name = strrchr(parent_path, '/');
    *dir_name = '\0';
    dir_name++;

    printf("parent\t%s\nfolder\t%s\n", parent_path, dir_name);
    struct inode *parent_inode = get_dir_inode(fs, parent_path);
    printf("found\t%d\n", parent_inode->nodeid);

    struct inode *inode = get_free_inode(fs);
    inode->direct1 = make_dir_file(fs, parent_inode->nodeid, inode->nodeid);

    inode->isDirectory = true;
    inode->file_size = 2 * sizeof(struct directory_item);

    add_file_to_dir(parent_inode, dir_name, inode->nodeid);
    return 0;
}


int main(int argc, const char* argv[]) {
    if(argc != 2){
        printf("Invalid number of arguments!\nUsage: inode <FS name>");
        exit(1);
    }

    FILE *file = fopen(argv[1], "r+");
    if (file == NULL){
        printf("File %s not found... Creating a new FS...", argv[1]);
    printf("ahoj");

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
    printf("size %d\n\n%d\n\n", sizeof(superblock), sizeof(struct inode));
    fs->inode_start_address = ((char *) fs->bitmap_start_address) + fs->cluster_count;
    fs->data_start_address = ((char *) fs->inode_start_address) + sizeof(struct inode) * fs->cluster_count;

    strcpy(fs->signature, "martin");
    strcpy(fs->volume_descriptor, "sample volume.");


    // add root folder
    struct inode *root_inode = get_free_inode(fs);
    root_inode->isDirectory = true;
    root_inode->nodeid = ROOT_ID;
    root_inode->direct1 = (int64_t) make_dir_file(fs, ROOT_ID, ROOT_ID);
    root_inode->file_size =  2 * sizeof(struct directory_item);

    mkdir(fs, "/dev");
    struct inode *dir_inode = get_dir_inode(fs, "/dev");
    int test = dir_inode->nodeid;
    printf("nalezeno:\t%d\n", test);

    mkdir(fs, "/dev/etc");
    dir_inode = get_dir_inode(fs, "/dev/etc");
    test = dir_inode->nodeid;
    printf("nalezeno:\t%d\n", test);

    dir_inode = get_dir_inode(fs, "");
    test = dir_inode->nodeid;
    printf("nalezeno:\t%d\n", test);

    return 0;
}