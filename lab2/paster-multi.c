/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The paster.c code is
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 license.
 */

/**
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include "zutil.h"
#include "crc.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>


#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

struct thread_args              /* thread input parameters struct */
{
    int* count;
    char* url;
};



size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len, int* count);
int concat_file();
void *get_files(void *arg);


/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
void *get_files(void *arg) {
  struct thread_args *p_in = arg;
  CURL *curl_handle;
  CURLcode res;
  char url[256];
  RECV_BUF recv_buf;
  char fname[256];

  while( *(p_in -> count) < 50) {
      recv_buf_init(&recv_buf, BUF_SIZE);

      strcpy(url, p_in -> url);

      curl_global_init(CURL_GLOBAL_DEFAULT);

      /* init a curl session */
      curl_handle = curl_easy_init();

      if (curl_handle == NULL) {
          fprintf(stderr, "curl_easy_init: returned NULL\n");
          return 1;
      }

      /* specify URL to get */
      curl_easy_setopt(curl_handle, CURLOPT_URL, url);

      /* register write call back function to process received data */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

      /* register header call back function to process received header data */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

      /* some servers requires a user-agent field */
      curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");


      /* get it! */
      res = curl_easy_perform(curl_handle);

      if( res != CURLE_OK) {
          fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      } else {
      printf("%lu bytes received in memory %p, seq=%d.\n", \
              recv_buf.size, recv_buf.buf, recv_buf.seq);
      }

      sprintf(fname, "./output_%d.png", recv_buf.seq);
      write_file(fname, recv_buf.buf, recv_buf.size, p_in -> count);

      /* cleaning up */
      curl_easy_cleanup(curl_handle);
      curl_global_cleanup();
      recv_buf_cleanup(&recv_buf);
  }

  return NULL;
}

int concat_file()
{
    int width;
    FILE* file_1 = fopen("./output_0.png", "rb");
    fseek(file_1, 16, SEEK_SET);
    fread(&width, 4, 1, file_1);
    fclose(file_1);
    char path[1000];

    width = htonl(width);

    /*Initialize buffers required to be updated as images are concatenated*/
    int height = 0;
    int i = 0;

    for (i = 0; i < 50; i++) {
        int image_height;
        FILE* image = fopen("./output_0.png", "rb");

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
    for (i = 0; i < 50; i++) {
        /*Update Image Height as Stacked PNG components are added*/
        sprintf(path,"./output_%d.png", i);
        FILE* image = fopen(path, "rb");


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

    file_1 = fopen("./output_0.png", "rb");
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
    FILE* all_png = fopen("output.png", "wb+");

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

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	    p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv,
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len, int* count)
{
    FILE *fp = NULL;
    if( access( path, F_OK ) != -1 ) {
        printf("File already exists!\n");
        return -1;
    }
    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }

    *count = *count + 1;
    return fclose(fp);
}


int main( int argc, char** argv )
{
    int c;
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";
    char url[1000];

    while ((c = getopt (argc, argv, "t:n:")) != -1) {
      switch (c) {
      case 't':
      t = strtoul(optarg, NULL, 10);
      printf("option -t specifies a value of %d.\n", t);
      if (t <= 0) {
        fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
        return -1;
      }
      break;
      case 'n':
      n = strtoul(optarg, NULL, 10);
      printf("option -n specifies a value of %d.\n", n);
      if (n <= 0 || n > 3) {
        fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
        return -1;
      }
      break;
      default:
        return -1;
      }
    }

    pthread_t *p_tids = malloc(sizeof(pthread_t) * t);
    int count = 0;

    struct thread_args in_params;

    in_params.count = &count;
    in_params.url = url;

    for (int i=0; i < t; i++) {
      sprintf(url,"http://ece252-%d.uwaterloo.ca:2520/image?img=%d", i % 3 + 1, n);
      pthread_create(p_tids + i, NULL, get_files, &in_params);
    }
    for (int i=0; i < t; i++) {
        pthread_join(p_tids[i], NULL);
        printf("Thread ID %lu joined.\n", p_tids[i]);
    }

    free(p_tids);

    int ret = concat_file();

    return 0;
}
