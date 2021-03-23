#include <stdio.h>
#include <stdlib.h>
#include "fat16reader.h"

int main(int argc, char* argv[]) {
    file_name = malloc(50);
    strcpy(file_name, argv[1]);

    Fat16BootSector bs;
    readblock(&bs, 0, BOOT_SIZE / SECTOR_SIZE); //read boot

    const size_t totalSectors = (bs.total_sectors_short > 0) ? bs.total_sectors_short : bs.total_sectors_long;

    uint16_t* table = (uint16_t*)malloc(bs.fat_size_sectors * SECTOR_SIZE);
    readblock(table, bs.reserved_sectors, bs.fat_size_sectors); //read fat

    const size_t rootDir = bs.reserved_sectors * SECTOR_SIZE + bs.fat_size_sectors * bs.number_of_fats * SECTOR_SIZE;
    const size_t otherDir = rootDir + bs.root_dir_entries * DIRENTRY_SIZE;

    Fat16DirEntry* root = (Fat16DirEntry*)malloc(bs.root_dir_entries * DIRENTRY_SIZE);
    readblock(root, rootDir / SECTOR_SIZE, DIRENTRY_SIZE * bs.root_dir_entries / SECTOR_SIZE); //read root
    if(dir_init(&bs, table, root) == 1) {
        printf("Failed to initialize directory");
        return 1;
    }

    size_t size = 0;
    char cmd[100];
    while(1) {
        pwd();
        fgets(cmd, 100, stdin);
        cmd[strcspn(cmd, "\n")] = 0;

        if(strcmp(cmd, "exit") == 0) {
            dir_free();
            free(table);
            free(root);
            free(file_name);
            return 0;
        }
        else if(strcmp(cmd, "pwd") == 0) {
            printf("Current directory: ");
            pwd();
            printf("\n");
        }

        else if(strcmp(cmd, "dir") == 0) {
            while(readdir(1) != NULL);
            printf("\n");
        }

        else if(strcmp(cmd, "rootinfo") == 0) {
            for(size_t i = 0; i < bs.root_dir_entries; ++i) {
                if(root[i].filename[0] != S_EMPTY && root[i].filename[0] != S_DEL && (root[i].attribute & F_LFN) != F_LFN)
                    ++size;
            }
            printf("Entries in root directory: %zu\nTotal available: %u\nPercentage: %.2f%%\n", size, bs.root_dir_entries, size * 100.0 / bs.root_dir_entries);
            printf("\n");
        }

        else if(strncmp(cmd, "cd", 2) == 0) {
            strtok(cmd, " ");
            char* temp = strtok(NULL, " ");
            if(strcmp(temp, "..") == 0)
                closedir();
            else
                if(opendir(temp)) printf("Directory not found\n");
            printf("\n");
        }

        else if(strncmp(cmd, "cat", 3) == 0) {
            strtok(cmd, " ");
            char* temp = strtok(NULL, " ");
            FILE_PTR* fp = open(temp);
            if(fp) {
                read(fp, NULL, fp->size);
                close(fp);
                printf("\n");
            }
            else printf("File not found\n");
            printf("\n");
        }

        else if(strncmp(cmd, "get", 3) == 0) {
            strtok(cmd, " ");
            char* temp = strtok(NULL, " ");
            FILE_PTR* fp = open(temp);
            if(fp) {
                size_t cluster_size = bs.sectors_per_cluster * SECTOR_SIZE;
                uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * cluster_size);
                FILE* f = fopen(fp->filename, "wb");
                size_t count = 0;
                if(f) {
                    while((count = read(fp, buffer, cluster_size)) != 0)
                        fwrite(buffer, sizeof(uint8_t), count, f);
                    fclose(f);
                }
                else printf("Couldn't create output file\n");
                close(fp);
                free(buffer);
            }
            else printf("File not found\n");
            printf("\n");
        }

        else if(strncmp(cmd, "zip", 3) == 0) {
            strtok(cmd, " ");

            char* temp = strtok(NULL, " ");
            FILE_PTR* fp1 = open(temp);
            temp = strtok(NULL, " ");
            FILE_PTR* fp2 = open(temp);

            if(fp1 && fp2 && fp1 != fp2) {
                temp = strtok(NULL, " ");
                char* buf1 = (char*)malloc(sizeof(uint8_t) * fp1->size);
                char* buf2 = (char*)malloc(sizeof(uint8_t) * fp2->size);
                read(fp1, buf1, fp1->size);
                read(fp2, buf2, fp2->size);
                FILE* f = fopen(temp, "w");
                if(f) {
                    char* save_ptr1;
                    char* save_ptr2;
                    char* tok1 = strtok_r(buf1, "\n", &save_ptr1);
                    char* tok2 = strtok_r(buf2, "\n", &save_ptr2);
                    fprintf(f, "%s\n", tok1);
                    fprintf(f, "%s\n", tok2);
                    while (tok1 != NULL || tok2 != NULL) {
                        if ((tok1 = strtok_r(NULL, "\n", &save_ptr1)) != NULL)
                            fprintf(f, "%s\n", tok1);
                        if ((tok2 = strtok_r(NULL, "\n", &save_ptr2)) != NULL)
                            fprintf(f, "%s\n", tok2);
                    }
                    fclose(f);
                }
                else printf("Couldn't create output file\n");
                close(fp1);
                close(fp2);
                free(buf1);
                free(buf2);
            }
            else printf("Input files error\n");
            printf("\n");
        }

        else if(strcmp(cmd, "spaceinfo") == 0) {
            size_t c_occupied = 0;
            size_t c_free = 0;
            size_t c_corrupted = 0;
            size_t c_EOC = 0;
            for(size_t i = 2; i < bs.fat_size_sectors * SECTOR_SIZE / 16; ++i) {
                if(table[i] == 0x0000) ++c_free;
                else if(table[i] >= 0x0002 && table[i] <= 0xFFEF) ++c_occupied;
                else if(table[i] == 0xFFF7) ++c_corrupted;
                else if(table[i] == 0xFFFF) ++c_EOC;
            }
            printf("Used clusters: %zu\nFree clusters: %zu\nCorrupted clusters: %zu\nChain-end clusters: %zu\nCluster size: %uB, %u sectors\n",
                    c_occupied, c_free, c_corrupted, c_EOC, bs.sectors_per_cluster * SECTOR_SIZE, bs.sectors_per_cluster);
            printf("\n");
        }

        else if(strncmp(cmd, "fileinfo", 8) == 0) {
            strtok(cmd, " ");
            char* temp = strtok(NULL, " ");
            FILE_PTR* fp = open(temp);
            if(fp) {
                print_fileinfo(fp);
                close(fp);
            }
            else printf("Couldn't find file\n");
            printf("\n");
        }
    }

    dir_free();
    free(table);
    free(root);
    free(file_name);
    return 0;
}