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

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

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
typedef struct recv_buf2 {
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
    int image_num;
	pthread_mutex_t count_mutex;
	pthread_mutex_t queue_mutex;
} g_vars;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(int seq, const void *in, size_t len, int* count);

void queue_init(queue* q, void* start, int buf_size);
void global_init(g_vars* g, int img);
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

void global_init(g_vars* g, int img) {
	g -> num_pics = 0;
    g -> image_num = img;
	pthread_mutex_init(&(g -> count_mutex), NULL);
	pthread_mutex_init(&(g -> queue_mutex), NULL);
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


void producer(queue* q, g_vars* g) {
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;

	while(1){
		pthread_mutex_lock(&(g -> count_mutex));

		int image_part = g -> num_pics;

		if (image_part == 50) {
			pthread_mutex_unlock(&(g -> count_mutex));
			break;
		}
		(g -> num_pics)++;

		pthread_mutex_unlock(&(g -> count_mutex));

        sprintf(url, "http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d", image_part % 3 + 1, g -> image_num, image_part);
        recv_buf_init(&recv_buf, BUF_SIZE);
        curl_global_init(CURL_GLOBAL_DEFAULT);

        /* init a curl session */
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
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
        }

        printf("image num: %d \n", recv_buf.seq);

        /* cleaning up */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        recv_buf_cleanup(&recv_buf);
    }
}

int write_file(int seq, const void *in, size_t len, int* count)
{
    return 0;
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
	int image_num = 1;

    int queue_id = shmget(IPC_PRIVATE, sizeof(queue) + sizeof(recv_chunk)*buf_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    void* temp_pointer = shmat(queue_id, NULL, 0);
    queue* queue_pointer = (queue*) temp_pointer;
    queue_init(queue_pointer, temp_pointer, buf_size);

	int global_id = shmget(IPC_PRIVATE, sizeof(g_vars), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	void* global_temp = shmat(global_id, NULL, 0);
	g_vars* var_pointer = (g_vars*) global_temp;
	global_init(var_pointer, image_num);

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
		else if ( main_pid == 0 ) {
            producer(queue_pointer, var_pointer);
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
