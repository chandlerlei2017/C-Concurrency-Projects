/**
 * @biref To demonstrate how to use zutil.c and crc.c functions
 */

#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <arpa/inet.h>

/*ECE 252 Lab 1 Group 1 Catpng Completed May 28 2019*/
int main (int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "Usage: %s <file1> <file2> <file3> ...\n", argv[0]);
        exit(1);
    }

    int width;
    FILE* file_1 = fopen(argv[1], "rb");
    fseek(file_1, 16, SEEK_SET);
    fread(&width, 4, 1, file_1);
    fclose(file_1);

    width = htonl(width);

    /*Initialize buffers required to be updated as images are concatenated*/
    int height = 0;
    int i = 0;

    for (i = 1; i <= argc - 1; i++) {
        int image_height;
        FILE* image = fopen(argv[i], "rb");

        /*Read-in image height, converting as required from network to system order*/
        fseek(image, 20, SEEK_SET);
        fread(&image_height, 4, 1, image);
        image_height = ntohl(image_height);

        /*Update all_png image height*/
        height = height + image_height;
    }

    U8 idat_data_buff[(width*4 + 1)* height];
    U64 total_data_length = 0;

    U8 compressed_data_buff[(width*4 + 1)* height];
    U64 total_compressed_length = 0;

    i = 0;
    /*Iterate through each png passed in as argument*/
    for (i = 1; i <= argc - 1; i++) {
        /*Update Image Height as Stacked PNG components are added*/
        FILE* image = fopen(argv[i], "rb");


        /* IDAT Chunk for each PNG */
        U64 idat_length;

        /* Seek to respective position, and read-in converting to system order as required*/
        fseek(image,33, SEEK_SET);
        fread(&idat_length, 4, 1, image);
        idat_length = ntohl(idat_length);

        /*Load data from IDAT into idat buffer for new png file*/
        U8 idat_buff[idat_length];
        fseek(image, 41, SEEK_SET);
        fread(idat_buff, idat_length, 1, image);
        fclose(image);

        U64 out_length = 0;
        int ret= 0;

        /*Perform decompression on IDATA data using memory_inflater*/
        ret = mem_inf(&(idat_data_buff[total_data_length]), &out_length, idat_buff, idat_length);

        if (ret != 0) { /* failure */
            fprintf(stderr,"mem_inf failed. ret = %d.\n", ret);
        }

        /*Update total length of PNG*/
        total_data_length += out_length;
    }

    /* For size of decompressed data:
        printf("height: %d, uncompressed data: %ld \n", height, total_data_length);
    */

    /*Perform data compression of IDATA for insertion into new PNGr*/
    int new_ret = 0;

    new_ret = mem_def(compressed_data_buff, &total_compressed_length, idat_data_buff, total_data_length, Z_DEFAULT_COMPRESSION);

    if (new_ret != 0) { /* failure */
        fprintf(stderr,"mem_inf failed. ret = %d.\n", new_ret);
    }
     /*Determine and set output compressed length*/
    height = htonl(height);
    U64 total_compressed_length_out = htonl(total_compressed_length);

    /* HEADER of ALL_PNG will be same as shown below so set */
    unsigned char header[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char ihdr_buffer[25];

    file_1 = fopen(argv[1], "rb");
    fseek(file_1, 8, SEEK_SET);
    fread(ihdr_buffer, 25, 1, file_1);
    fclose(file_1);

    /* Set iHEADER components for ALL_PNG */
    unsigned char iend[4] = {'I', 'E', 'N', 'D'};
    unsigned char idat[4] = {'I', 'D', 'A', 'T'};
    unsigned char ihdr_crc_in[17];
    unsigned char idat_crc_in[total_compressed_length + 4];
    int ihdr_crc_out;
    int idat_crc_out;
    int iend_crc_out;
    int iend_length = 0;

    /*Create new file all_png */
    FILE* all_png = fopen("all.png", "wb+");

    /* Write header, iHeader, iData chunks as required into all_png */
    fwrite(header, 8, 1, all_png);
    fwrite(ihdr_buffer, 25, 1, all_png);

    fseek(all_png, 20, SEEK_SET);
    fwrite(&height, 4, 1, all_png);

    fseek(all_png, 12, SEEK_SET);
    fread(ihdr_crc_in, 17, 1, all_png);

    /* Perform CRC application on type and data fields of ihdr */
    ihdr_crc_out = htonl(crc(ihdr_crc_in, 17));
    fwrite(&ihdr_crc_out, 4, 1, all_png);

    fwrite(&total_compressed_length_out, 4, 1, all_png);
    fwrite(idat, 4, 1, all_png);
    fwrite(compressed_data_buff, total_compressed_length, 1, all_png);

    fseek(all_png, 37, SEEK_SET);
    fread(idat_crc_in, total_compressed_length + 4, 1, all_png);

    /* Perform CRC application on type and data fields of IDATA */
    idat_crc_out = htonl(crc(idat_crc_in, total_compressed_length + 4));
    fwrite(&idat_crc_out, 4, 1, all_png);

    fwrite(&iend_length, 4, 1, all_png);
    fwrite(iend, 4, 1, all_png);

    /* Perform CRC application on IEND chunk type and data fields*/
    iend_crc_out = htonl(crc(iend, 4));
    fwrite(&iend_crc_out, 4, 1, all_png);

    /* Close completed new png and return */
    fclose(all_png);

    return 0;
}
