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
    FILE  *f;
    int   *fat_data;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL)
    {
        fprintf(stderr, "usage: statuvfs --image <imagename>\n");
        exit(1);
    }

    // read the binary file
    f = fopen(imagename, "r");
    fseek(f, 0 , SEEK_SET);
    fread(&sb, sizeof(sb), 1, f);

    // little endian --> big endian
    sb.block_size = htons(sb.block_size);
    sb.num_blocks = htonl(sb.num_blocks);
    sb.fat_start = htonl(sb.fat_start);
    sb.fat_blocks = htonl(sb.fat_blocks);
    sb.dir_start = htonl(sb.dir_start);
    sb.dir_blocks = htonl(sb.dir_blocks);

    // print information
    printf("%s (%s)\n\n", sb.magic, imagename);
    printf("-----------------------------------------------------------\n");
    printf("  %s %7s %7s %7s %7s %7s \n", "Bsz", "Bcnt", "FATst", "FATcnt", "DIRst", "DIRcnt");

    printf("  %d %7d %7d %7d %7d %7d \n",
                   sb.block_size,
                   sb.num_blocks,
                   sb.fat_start,
                   sb.fat_blocks,
                   sb.dir_start,
                   sb.dir_blocks);
    printf("-----------------------------------------------------------\n\n");

    // go through FAT to find stats
    int free_b = 0;
    int res_b = 0;
    int alloc_b = 0;

    fseek(f, sb.fat_start * sb.block_size, SEEK_SET);
    for(i = 0; i < sb.num_blocks; i++){
        fread(&fat_data, 4, 1, f);
        fat_data = htonl(fat_data);
        if(fat_data == 0) free_b++;
        else if(fat_data == 1) res_b++;
        else if(fat_data <= 0xffffff00 || fat_data == 0xffffffff) alloc_b++;
    }

    printf(" %s %7s %7s\n", "Free", "Resv", "Alloc");
    printf(" %d %7d %7d\n", free_b, res_b, alloc_b);

    return 0;
}
