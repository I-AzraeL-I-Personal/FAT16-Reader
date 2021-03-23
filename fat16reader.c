#include "fat16reader.h"

int strncicmp(const char* str1, const char* str2, size_t n) {
    if(n == 0) {
        size_t len = strlen(str1);
        if(strlen(str2) != len)
            return 1;
        n = len;
    }
    for(size_t i = 0; i < n; ++i) {
        if(toupper(str1[i]) != toupper(str2[i]))
            return 1;
    }
    return 0;
}

size_t readblock(void* buffer, uint32_t first_block, size_t block_count) {
    FILE* fp = fopen(file_name, "rb");
    fseek(fp, first_block * SECTOR_SIZE, SEEK_SET);
    size_t count = fread(buffer, SECTOR_SIZE, block_count, fp);
    fclose(fp);
    return count;
}

void print_entry(Fat16DirEntry entry) {
    if(entry.filename[0] == S_DEL || entry.filename[0] == S_EMPTY || (entry.attribute & F_LFN) == F_LFN)
        return;

    printf("%02u/%02u/%u %02u:%02u:%02u ", entry.date & 31U, ((entry.date >> 5U) & 15U), 1980U + (entry.date >> 9U),
           entry.time >> 11U, (entry.time >> 5U) & 63U, (entry.time && 31) * 2U);
    ((entry.attribute & F_DIR) == F_DIR) ? printf("<DIRECTORY> ") : printf("%*u ", 11, entry.file_size);

    for(int i = 0; i < 8; ++i) {
        if(!isalnum(entry.filename[i]))
            break;
        putchar(entry.filename[i]);
    }

    if((entry.attribute & F_DIR) != F_DIR && entry.extension[0] != ' ')
        printf(".%s", entry.extension);
    printf("\n");
}

void print_fileinfo(FILE_PTR* fp) {
    printf("Full path: ");
    pwd();
    printf("%s\n", fp->filename);
    printf("Attributes:");
    ((fp->attributes >> 5U) & 1U) ? printf(" A+") : printf(" A-");
    ((fp->attributes >> 4U) & 1U) ? printf(" R+") : printf(" R-");
    ((fp->attributes >> 3U) & 1U) ? printf(" S+") : printf(" S-");
    ((fp->attributes >> 2U) & 1U) ? printf(" H+") : printf(" H-");
    ((fp->attributes >> 1U) & 1U) ? printf(" D+") : printf(" D-");
    (fp->attributes & 1U) ? printf(" D+") : printf(" V-");
    printf("\nSize: %zuB\n", fp->size);
    printf("Date: %02u/%02u/%u %02u:%02u:%02u\n", fp->date & 31U, ((fp->date >> 5U) & 15U), 1980U + (fp->date >> 9U),
           fp->time >> 11U, (fp->time >> 5U) & 63U, (fp->time && 31) * 2U);
    printf("Cluster chain: [%u]", fp->first_cluster);
    uint16_t cluster = dir->table[fp->first_cluster];
    size_t c_number = 1;
    while(cluster != EOC) {
        printf(", %u", cluster);
        cluster = dir->table[cluster];
        ++c_number;
    }
    printf("\nNumber of clusters: %zu\n", c_number);
}

int dir_init(const Fat16BootSector* bs, const uint16_t* table, const Fat16DirEntry* root) {
    dir = (DIR*)malloc(sizeof(DIR));
    if(!dir)
        return 1;

    dir->head = (struct dir_node_t*)malloc(sizeof(struct dir_node_t));
    if(!dir->head) {
        free(dir);
        return 1;
    }
    dir->bs = bs;
    dir->table = table;
    dir->root = root;
    strcpy(dir->head->dir_data.filename, "ROOT");
    dir->head->dir_data.first_cluster = 0;
    dir->head->dir_data.size = 0;
    dir->tail = dir->head;
    return 0;
}

void dir_free() {
    struct dir_node_t* temp;
    while(dir->tail) {
        temp = dir->tail->prev;
        free(dir->tail);
        dir->tail = temp;
    }
    free(dir);
}

void pwd() {
    struct dir_node_t* temp = dir->head->next;
    printf("\\");
    while(temp) {
        printf("%s\\", temp->dir_data.filename);
        temp = temp->next;
    }
}

int opendir(const char* name) {
    FILE_PTR* iter;
    while((iter = readdir(0)) != NULL) {
        if(strncicmp(name, iter->filename, 0) == 0 && iter->type == 1) {
            struct dir_node_t* new = (struct dir_node_t*)malloc(sizeof(struct dir_node_t));
            new->dir_data.first_cluster = iter->first_cluster;
            strcpy(new->dir_data.filename, name);
            new->dir_data.size = 0;
            new->next = NULL;
            new->prev = dir->tail;
            dir->tail->next = new;
            dir->tail = new;
            rewinddir();
            return 0;
        }
    }
    return -1;
}

FILE_PTR* readdir(int mode) {
    static FILE_PTR last_record;
    static size_t counter;
    size_t data_dir = dir->bs->reserved_sectors * SECTOR_SIZE + dir->bs->fat_size_sectors * dir->bs->number_of_fats * SECTOR_SIZE + dir->bs->root_dir_entries * DIRENTRY_SIZE;
    uint16_t cluster = dir->tail->dir_data.first_cluster;
    size_t cluster_size = dir->bs->sectors_per_cluster * SECTOR_SIZE;

    Fat16DirEntry* entry;
    size_t localCounter = 0;
    if(dir->tail == dir->head) {
        for(size_t i = 0; i < dir->bs->root_dir_entries; ++i) {
            if(dir->root[i].filename[0] == S_DEL || dir->root[i].filename[0] == S_EMPTY || (dir->root[i].attribute & F_LFN) == F_LFN)
                continue;
            ++localCounter;
            if(localCounter > counter) {
                memset(last_record.filename, 0, 13);
                for(size_t j = 0; j < 8; ++j) {
                    if(dir->root[i].filename[j] == ' ') break;
                    last_record.filename[j] = dir->root[i].filename[j];
                }
                last_record.type = 1;
                if((dir->root[i].attribute & F_DIR) != F_DIR) {
                    last_record.type = 0;
                    if(dir->root[i].extension[0] != ' ') {
                        strcat(last_record.filename, ".");
                        size_t len = strlen(last_record.filename);
                        for(size_t j = 0; j < 3; ++j) {
                            if(dir->root[i].extension[j] == ' ') break;
                            last_record.filename[len + j] = dir->root[i].extension[j];
                        }
                    }
                }
                last_record.size = dir->root[i].file_size;
                last_record.first_cluster = dir->root[i].first_cluster;
                last_record.offset = 0;
                last_record.attributes = dir->root[i].attribute;
                last_record.date = dir->root[i].date;
                last_record.time = dir->root[i].time;
                if(mode == 1) { //print info mode
                    print_entry(dir->root[i]);
                }
                ++counter;
                return &last_record;
            }
        }
    }
    else {
        uint8_t* buf = (uint8_t*)calloc(cluster_size, sizeof(uint8_t));
        while(cluster != EOC) {
            readblock(buf, (data_dir + cluster_size * (cluster - 2)) / SECTOR_SIZE, cluster_size / SECTOR_SIZE);
            for(size_t i = 0; i < cluster_size / DIRENTRY_SIZE; ++i) {
                entry = (Fat16DirEntry*)buf + i;
                if(entry->filename[0] != S_DEL && entry->filename[0] != S_EMPTY && (entry->attribute & F_LFN) != F_LFN && entry->filename[0] != '.') {
                    ++localCounter;
                    if(localCounter > counter) {
                        memset(last_record.filename, 0, 13);
                        for(size_t j = 0; j < 8; ++j) {
                            if(entry->filename[j] == ' ') break;
                            last_record.filename[j] = entry->filename[j];
                        }
                        last_record.type = 1;
                        if((entry->attribute & F_DIR) != F_DIR) {
                            last_record.type = 0;
                            if(entry->extension[0] != ' ') {
                                strcat(last_record.filename, ".");
                                size_t len = strlen(last_record.filename);
                                for (size_t j = 0; j < 3; ++j) {
                                    if (entry->extension[j] == ' ') break;
                                    last_record.filename[len + j] = entry->extension[j];
                                }
                            }
                        }
                        last_record.size = entry->file_size;
                        last_record.first_cluster = entry->first_cluster;
                        last_record.offset = 0;
                        last_record.attributes = entry->attribute;
                        last_record.date = entry->date;
                        last_record.time = entry->time;
                        if(mode == 1) { //print info mode
                            print_entry(*entry);
                        }
                        ++counter;
                        free(buf);
                        return &last_record;
                    }
                }
            }
            cluster = dir->table[cluster];
        }
        free(buf);
    }
    counter = 0;
    return NULL;
}

int closedir() {
    if(dir->tail->dir_data.first_cluster == 0) //root
        return 1;
    struct dir_node_t* temp = dir->tail->prev;
    free(dir->tail);
    dir->tail = temp;
    dir->tail->next = NULL;
    return 0;
}

void rewinddir() {
    while(readdir(0) != NULL);
}

FILE_PTR* open(const char* name) {
    FILE_PTR* file = NULL;
    FILE_PTR* iter;
    while((iter = readdir(0)) != NULL) {
        if(strncicmp(name, iter->filename, 0) == 0 && iter->type == 0) {
            file = (FILE_PTR*)malloc(sizeof(FILE_PTR));
            file->first_cluster = iter->first_cluster;
            file->size = iter->size;
            file->offset = 0;
            file->time = iter->time;
            file->attributes = iter->attributes;
            file->date = iter->date;
            strcpy(file->filename, name);
            rewinddir();
            return file;
        }
    }
    return NULL;
}

int close(FILE_PTR* fp) {
    if(!fp)
        return -1;
    free(fp);
    return 0;
}

size_t read(FILE_PTR* fd, void* buffer, size_t count) {
    if(!fd || !count)
        return 0;
    size_t data_dir = dir->bs->reserved_sectors * SECTOR_SIZE + dir->bs->fat_size_sectors * dir->bs->number_of_fats * SECTOR_SIZE + dir->bs->root_dir_entries * DIRENTRY_SIZE;
    uint16_t cluster = fd->first_cluster;
    size_t cluster_size = dir->bs->sectors_per_cluster * SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)calloc(cluster_size, sizeof(uint8_t));

    if(fd->offset == fd->size) {
        frewind(fd);
        return 0;
    }
    size_t count_ret = 0;
    if(count > fd->size)
        count = fd->size;
    while(count) {
        if(cluster == EOC)
            break;
        readblock(buf, (data_dir + cluster_size * (cluster - 2)) / SECTOR_SIZE, cluster_size / SECTOR_SIZE);
        if(count >= cluster_size) {
            if(buffer && count > fd->offset) {
                memcpy(buffer + count_ret, buf, cluster_size);
                fd->offset += cluster_size;
            }
            if(!buffer) {
                for(size_t i = 0; i < cluster_size; ++i)
                    printf("%c", buf[i]);
            }
            count_ret += cluster_size;
            count -= cluster_size;
        }
        else {
            if(buffer && count > fd->offset) {
                memcpy(buffer, buf, count);
                fd->offset += count;
            }
            if(!buffer) {
                for(size_t i = 0; i < count; ++i)
                    printf("%c", buf[i]);
            }
            count_ret += count;
            count = 0;
        }
        cluster = dir->table[cluster];
    }
    free(buf);
    return count_ret;
}

void frewind(FILE_PTR* fd) {
    fd->offset = 0;
}






