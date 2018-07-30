#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "disk.h"


int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename = NULL;
    char *filename  = NULL;
    FILE *f;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            filename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL || filename == NULL) {
        fprintf(stderr, "usage: catuvfs --image <imagename> " \
            "--file <filename in image>");
        exit(1);
    }

    // read the binary file
    f = fopen(imagename, "r");
    fseek(f, 0, SEEK_SET);
    fread(&sb, sizeof(sb), 1, f);

    // go from little endian to big endian
    sb.block_size = htons(sb.block_size);
    sb.num_blocks = htonl(sb.num_blocks);
    sb.fat_start = htonl(sb.fat_start);
    sb.fat_blocks = htonl(sb.fat_blocks);
    sb.dir_start = htonl(sb.dir_start);
    sb.dir_blocks = htonl(sb.dir_blocks);

    fseek(f, sb.dir_start * sb.block_size, SEEK_SET);

    // find the correct file
    int num_entries = sb.dir_blocks * (sb.block_size / 64);
    directory_entry_t dir;
    char* found = NULL;
    for(i = 0; i < num_entries; i++){
        fread(&dir, sizeof(directory_entry_t), 1, f);
        if(dir.status != DIR_ENTRY_AVAILABLE){
            if(strncmp(dir.filename, filename, strlen(filename) + 1) == 0){
                found = filename;
                break;
            }
        }
    }

    if(found == NULL){
        printf("File not found. \n");
        exit(-1);
    } else {
        dir.start_block = htonl(dir.start_block);
        dir.num_blocks = htonl(dir.num_blocks);
        dir.file_size = htonl(dir.file_size);

        int current_block = dir.start_block;
        int left_to_read = dir.file_size;
        int fat_val;
        int values_read;
        char buffer[sb.block_size];
        while(1){

            // read block contents
            fseek(f, current_block * sb.block_size, SEEK_SET);
            if(left_to_read - sb.block_size < 0) values_read = fread(buffer, 1, left_to_read, f);
            else values_read = fread(buffer, 1, sb.block_size, f);
            printf("%.*s", values_read, buffer);
            left_to_read = left_to_read - values_read;

            // next block_size
            fseek(f, sb.fat_start * sb.block_size + (4 * current_block), SEEK_SET);
            if( fread(&fat_val, 4, 1, f) == -1 ) break;
            fat_val = htonl(fat_val);
            current_block = fat_val;
            if(current_block == FAT_LASTBLOCK) break;

        }
    }

    return 0;
}
