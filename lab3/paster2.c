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
 * @file main.c
 * @brief cURL write call back to save received data in a shared memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 * NOTE: we assume each image segment from the server is less than 10K
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include "zutil.h"
#include "crc.h"

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240 /* 1024*10 = 10K */

/* This is a flattened structure, buf points to
   the memory address immediately after
   the last member field (i.e. seq) in the structure.
   Here is the memory layout.
   Note that the memory is a chunk of continuous bytes.

   +================+
   | buf            | 8 bytes
   +----------------+
   | size           | 4 bytes
   +----------------+
   | max_size       | 4 bytes
   +----------------+
   | seq            | 4 bytes
   +----------------+
   | buf[0]         | 1 byte
   +----------------+
   | buf[1]         | 1 byte
   +----------------+
   + ...            | 1 byte
   +----------------+
   + buf[max_size-1]| 1 byte
   +================+
*/
typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

typedef struct recv_chunk {
    char buf[10000];
    size_t size;
	size_t max_size;
    int seq;
	
} recv_chunk;

typedef struct queue {
    recv_chunk* buf;
    int curr_size;
    int max_size;
    int front;
    int back;
} queue;

typedef struct g_vars {
	int num_pics;
	pthread_mutex_t count_mutex;
	pthread_mutex_t queue_mutex;
} g_vars;



size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(recv_chunk *ptr);
int write_file(const char *path, const void *in, size_t len);
void queue_init(queue* q, void* start, int buf_size);
void global_init(g_vars* g);
void producer(queue* q, g_vars* g);

void queue_init(queue* q, void* start, int buf_size) {
    q -> buf = (recv_chunk*) (start + sizeof(queue));
    q -> curr_size = 0;
    q -> max_size = buf_size;
    q -> front = 0;
    q -> back = 0;

    // Init eac
    for (int i = 0; i < buf_size; i++) {
        recv_chunk* temp_recv = q -> buf + i;
        temp_recv -> size = 0;
        temp_recv -> seq = -1;
    }
}

void global_init(g_vars* g) {
	g -> num_pics = 0;
	pthread_mutex_init(&(g -> count_mutex), NULL);
	pthread_mutex_init(&(g -> queue_mutex), NULL);
}

void producer(queue* q, g_vars* g) {
	CURL *curl_handle;
	CURLcode res;
	char url[256];
	recv_chunk img_chunk;
		
	while(1){
		shm_recv_buf_init(&img_chunk, BUF_SIZE);
		pthread_mutex_lock(&(g -> count_mutex));
		int imageNum = g -> num_pics;
		
		if (imageNum == 50) {
			pthread_mutex_unlock(&(g -> count_mutex));
			break;
		}
		(g -> num_pics)++;
		pthread_mutex_unlock(&(g -> count_mutex));	
		
		curl_global_init(CURL_GLOBAL_DEFAULT);
		curl_handle = curl_easy_init();
		
		if (curl_handle == NULL) {
          fprintf(stderr, "curl_easy_init: returned NULL\n");
		}
		
		/* specify URL to get */
	    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

	    /* register write call back function to process received data */
	    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
	    /* user defined data structure passed to the call back function */
	    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&img_chunk);

	    /* register header call back function to process received header data */
	    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
	    /* user defined data structure passed to the call back function */
	    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&img_chunk);

	    /* some servers requires a user-agent field */
	    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");


	    /* get it! */
	    res = curl_easy_perform(curl_handle);

	    if( res != CURLE_OK) {
		  fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	    }

	    //write_file(img_chunk.seq, img_chunk.buf, img_chunk.size, p_in -> count);

	    /* cleaning up */
	    curl_easy_cleanup(curl_handle);
    	curl_global_cleanup();
	    recv_buf_cleanup(&img_chunk);
	}
}

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
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    recv_chunk *p = userdata;

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

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    recv_chunk *p = (recv_chunk *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

/**
 * @brief calculate the actual size of RECV_BUF
 * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
 * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
 */
int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure.
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }

    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */

    return 0;
}

int recv_buf_cleanup(recv_chunk *ptr)
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

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

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
    return fclose(fp);
}


int main( int argc, char** argv )
{	
	/*
    int buf_size = atoi(argv[1]);
	int num_prod = atoi(argv[2]);
	int num_con = atoi(argv[3]);
	int sleep_time = atoi(argv[4]);
	int num_image = atoi(argv[5]);
	*/
	int buf_size = 5;
	int num_prod = 5;
	int num_con = 5;
	int sleep_time = 5;
	int num_image = 5;
	
    int queue_id = shmget(IPC_PRIVATE, sizeof(queue) + sizeof(recv_chunk)*buf_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    void* temp_pointer = shmat(queue_id, NULL, 0);
    queue* queue_pointer = (queue*) temp_pointer;
    queue_init(queue_pointer, temp_pointer, buf_size);
	
	int global_id = shmget(IPC_PRIVATE, sizeof(g_vars), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	void* global_temp = shmat(global_id, NULL, 0);
	g_vars* var_pointer = (g_vars*) global_temp;
	global_init(var_pointer);
	
	/*
	for (int i = 0; i < queue_pointer -> max_size; i++) {
		recv_chunk* temp = queue_pointer -> buf + i;
		printf("seq: %d, size: %d \n", temp -> seq, temp -> size);
	}
	*/ 
	
	pid_t main_pid;
	pid_t cpids[num_con + num_prod];
	int state;
	
	// consumers
	for( int i = 0; i < num_con; i++) {
		
		main_pid = fork();
		
		if ( main_pid > 0 ) {
			cpids[i] = main_pid;
		}
		else if ( main_pid == 0 ) {
			return 0;
		}
		else {
			perror("fork");
			exit(3);
		}
	}
	
	// producers
	for( int i = num_con; i < num_con + num_prod; i++ ) {
		
		main_pid = fork();
		
		if ( main_pid > 0 ) {
			cpids[i] = main_pid;
		}
		else if ( main_pid == 0 ){
			return 0;
		}
		else {
			perror("fork");
			exit(3);
		}
	}
	
	// parent
	if ( main_pid > 0 ) {
		for ( int i = 0; i < num_con + num_prod; i++ ) {
			waitpid(cpids[i], &state, 0);
			if (WIFEXITED(state)) {
                printf("Child cpid[%d]=%d terminated with state: %d.\n", i, cpids[i], state);
            }
		}
	}
	
	return 0;
}
