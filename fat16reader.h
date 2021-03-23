#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#define BOOT_SIZE (sizeof(Fat16BootSector))
#define DIRENTRY_SIZE (sizeof(Fat16DirEntry))
#define SECTOR_SIZE 512

#define EOC 0xffffU       //END OF CHAIN
#define S_EMPTY 0x00U     //STATE EMPTY
#define S_DEL 0xe5U      //STATE DELETED
#define F_LFN 0x0fU      //FLAG LONG FILENAME
#define F_DIR 0x10U      //FLAG DIRECTORY


char* file_name;

typedef struct {
    uint8_t jump[3];
    unsigned char oem[8];
    uint16_t sector_size;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors_short;
    uint8_t media_descriptor;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;

    uint16_t drive_number;
    uint8_t extended_signature;
    uint32_t serial_number;
    char volume_name[11];
    char fat_name[8];
    uint8_t exe_code[448];
    uint16_t exe_marker;
} __attribute((packed)) Fat16BootSector;

typedef struct {
    unsigned char filename[8];
    unsigned char extension[3];
    uint8_t attribute;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute((packed)) Fat16DirEntry;

typedef struct {                    //file pointer returned by open() and used by opendir()
    uint16_t first_cluster;
    uint8_t attributes;
    uint16_t date;
    uint16_t time;
    size_t size;
    size_t offset;
    unsigned char filename[13];
    bool type;                      //0 - file, 1 - directory
} FILE_PTR;

struct dir_node_t {
    FILE_PTR dir_data;
    struct dir_node_t* next;
    struct dir_node_t* prev;
};

typedef struct {                    //directory dll
    const Fat16BootSector* bs;
    const uint16_t* table;
    const Fat16DirEntry* root;
    struct dir_node_t* head;
    struct dir_node_t* tail;
} DIR;

DIR* dir;                           //global pointer storing current directory list

size_t readblock(void* buffer, uint32_t first_block, size_t block_count);
void print_entry(Fat16DirEntry entry);
void print_fileinfo(FILE_PTR* fp);
int strncicmp(const char* str1, const char* str2, size_t n);

int dir_init(const Fat16BootSector* bs, const uint16_t* table, const Fat16DirEntry* root);
void dir_free();
void pwd();
int opendir(const char* name);
FILE_PTR* readdir(int mode);
int closedir();
void rewinddir();

FILE_PTR* open(const char* name);
int close(FILE_PTR* fp);
size_t read(FILE_PTR* fd, void* buffer, size_t count);
void frewind();


