#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "disk.h"


/*
 * Based on http://bit.ly/2vniWNb
 */
void pack_current_datetime(unsigned char *entry) {
    assert(entry);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    unsigned short year   = tm.tm_year + 1900;
    unsigned char  month  = (unsigned char)(tm.tm_mon + 1);
    unsigned char  day    = (unsigned char)(tm.tm_mday);
    unsigned char  hour   = (unsigned char)(tm.tm_hour);
    unsigned char  minute = (unsigned char)(tm.tm_min);
    unsigned char  second = (unsigned char)(tm.tm_sec);

    year = htons(year);

    memcpy(entry, &year, 2);
    entry[2] = month;
    entry[3] = day;
    entry[4] = hour;
    entry[5] = minute;
    entry[6] = second;
}


int next_free_block(int *FAT, int max_blocks) {
    assert(FAT != NULL);

    int i;

    for (i = 0; i < max_blocks; i++) {
        if (FAT[i] == FAT_AVAILABLE) {
            return i;
        }
    }

    return -1;
}


int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename  = NULL;
    char *filename   = NULL;
    char *sourcename = NULL;
    FILE *f;
    FILE *s;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            filename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--source") == 0 && i+1 < argc) {
            sourcename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL || filename == NULL || sourcename == NULL) {
        fprintf(stderr, "usage: storuvfs --image <imagename> " \
            "--file <filename in image> " \
            "--source <filename on host>\n");
        exit(1);
    }

    // read the host-system file
    f = fopen(sourcename, "r");
    if( f == NULL ){
        printf("Host-system file cannot be found.\n");
        exit(-1);
    }
    fseek(f, 0, SEEK_SET);

    // see if image already exists
    s = fopen(imagename, "r+");

    // image exists, check if sourcefile is already stored
    if( s != NULL ){
        printf("opened imagefile.\n");
        fseek(s, 0, SEEK_SET);
        fread(&sb, sizeof(sb), 1, s);

        // go from little endian to big endian
        sb.block_size = htons(sb.block_size);
        sb.num_blocks = htonl(sb.num_blocks);
        sb.fat_start = htonl(sb.fat_start);
        sb.fat_blocks = htonl(sb.fat_blocks);
        sb.dir_start = htonl(sb.dir_start);
        sb.dir_blocks = htonl(sb.dir_blocks);

        fseek(s, sb.dir_start * sb.block_size, SEEK_SET);

        // see if the sourcefile already exists
        int num_entries = sb.dir_blocks * (sb.block_size / 64);
        directory_entry_t dir;
        dir.start_block = htonl(dir.start_block);
        dir.num_blocks = htonl(dir.num_blocks);
        dir.file_size = htonl(dir.file_size);
        char* found = NULL;

        for(i = 0; i < num_entries; i++){
            fread(&dir, sizeof(directory_entry_t), 1, s);
            if(dir.status != DIR_ENTRY_AVAILABLE){
                if(strncmp(dir.filename, sourcename, strlen(sourcename) + 1) == 0){
                    found = sourcename;
                    break;
                }
            }
        }

        if( found == NULL){
            // sourcefile isn't in the image, we can add it to the image
            printf("adding to image... \n");

            // find file size of new sourcefile
            int size;
            fseek(f, 0L, SEEK_END);
            size = ftell(f);
            fseek(f, 0L, SEEK_SET);

            /*
            no need to change the superblock when adding a file

            // big endian --> little endian
            sb.block_size = ntohs(sb.block_size);
            sb.num_blocks = ntohl(sb.num_blocks);
            sb.fat_start = ntohl(sb.fat_start);
            sb.fat_blocks = ntohl(sb.fat_blocks);
            sb.dir_start = ntohl(sb.dir_start);
            sb.dir_blocks = ntohl(sb.dir_blocks);

            // write new superblock
            fseek(s, 0, SEEK_SET);
            fwrite(&sb, sizeof(sb), 1, s);

            */

            // now edit the FAT
            fseek(s, dir.start_block * sb.block_size, SEEK_SET);
            for(i = 0; i < num_entries; i++){
                fread(&dir, sizeof(directory_entry_t), 1, s);
                dir.file_size = htonl(dir.file_size);
                if(dir.status == DIR_ENTRY_AVAILABLE) break;
            }
            // once we exit the loop, dir is the at an empty entry

            // update the new FAT entry
            dir.file_size = size;
            dir.file_size = ntohl(dir.file_size);
            dir.status = FAT_RESERVED; 
            strcpy(dir.filename, filename);
            pack_current_datetime(dir.create_time);
            pack_current_datetime(dir.modify_time);

            // write the new FAT entry
            fwrite(&dir, sizeof(directory_entry_t), 1, s);

            // write the data block for the actual sourcefile
            fseek(s, (sb.fat_start + dir.start_block) * sb.block_size, SEEK_SET);
            fwrite(s, size, 1, f);

        } else {
            printf("File already exists in the disk image.\n");
        }

    } else {
        // imagefile doesn't exist yet, we can create it
        printf("Creating new image... \n");
    }

    return 0;
}
