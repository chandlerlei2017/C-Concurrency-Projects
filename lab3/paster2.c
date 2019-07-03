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
#include <time.h>
#include <sys/time.h>
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
    pthread_mutex_t p_count_mutex;
    pthread_mutex_t c_count_mutex;
	pthread_mutex_t queue_mutex;
    pthread_mutex_t idat_mutex;
    sem_t empty;
    sem_t full;
	int pics_prod;
    int pics_cons;
    int image_num;
    int next_prod;
    int next_cons;
} g_vars;

typedef struct idat_chunk {
    U8* buf;
    int size;
} idat_chunk;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);

void queue_init(queue* q, void* start, int buf_size);
void global_init(g_vars* g, int img, int buf_size);
void producer(queue* q, g_vars* g);
void consumer(queue* q, g_vars* g, idat_chunk* idat_buf, int time);
void enqueue(queue* q, const void *in, size_t len, int seq);
void idat_init(idat_chunk* idat_pointer, void* start);
void concat_image(idat_chunk* idat_pointer, queue* q);

void queue_init(queue* q, void* start, int buf_size) {
    q -> buf = (recv_chunk*) (start + sizeof(queue));
    q -> curr_size = 0;
    q -> max_size = buf_size;
    q -> front = 0;
    q -> back = -1;

    // Init each recv chunk
    for (int i = 0; i < buf_size; i++) {
        recv_chunk* temp_recv = q -> buf + i;
        temp_recv -> size = 0;
        temp_recv -> seq = -1;
    }
}

void global_init(g_vars* g, int img, int buf_size) {
	g -> pics_prod = 0;
    g -> pics_cons = 0;
    g -> image_num = img;
    g -> next_prod = 0;
    g->  next_cons = 0;
	pthread_mutex_init(&(g -> p_count_mutex), NULL);
    pthread_mutex_init(&(g -> c_count_mutex), NULL);
	pthread_mutex_init(&(g -> queue_mutex), NULL);
    pthread_mutex_init(&(g -> idat_mutex), NULL);
    sem_init(&(g -> empty), 1, buf_size);
    sem_init(&(g -> full), 1, 0);
}
void enqueue(queue* q, const void *in, size_t len, int seq) {
    if (q -> curr_size != q -> max_size) {
        (q -> curr_size)++;
    }
    else {
        (q -> front) = (q -> front + 1) % q -> max_size;
    }
    q -> back = (q -> back + 1) % q -> max_size;

    recv_chunk* curr_chunk = q -> buf + q -> back;
    curr_chunk -> seq = seq;
    curr_chunk -> size = len;
    memcpy(curr_chunk -> buf, in, len);
}

void idat_init(idat_chunk* idat_pointer, void* start) {
    idat_pointer -> size = 0;
    idat_pointer -> buf = (U8*) (start + sizeof(idat_chunk));
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

	while(1) {
		pthread_mutex_lock(&(g -> p_count_mutex));

		int image_part = g -> pics_prod;
		(g -> pics_prod)++;

		pthread_mutex_unlock(&(g -> p_count_mutex));

        if (image_part > 49) {
            break;
        }
        else {
            sem_wait(&(g -> empty));
        }

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

        // Write to queue
        while(g -> next_prod != recv_buf.seq) {
        }

        pthread_mutex_lock(&(g -> queue_mutex));

        enqueue(q, recv_buf.buf, recv_buf.size, recv_buf.seq);

        (g -> next_prod)++;

        pthread_mutex_unlock(&(g -> queue_mutex));

        /* cleaning up */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        recv_buf_cleanup(&recv_buf);

        sem_post(&(g -> full));
    }
}

void consumer(queue* q, g_vars* g, idat_chunk* idat, int time) {
    while(1) {
        pthread_mutex_lock(&(g -> c_count_mutex));

		int image_part = g -> pics_cons;
		(g -> pics_cons)++;

		pthread_mutex_unlock(&(g -> c_count_mutex));

        if(image_part > 49) {
            break;
        }
        else {
            sem_wait(&(g -> full));
            usleep(time*1000);
        }

        recv_chunk* temp = q -> buf + (image_part % q -> max_size);

        while (temp -> seq != g -> next_cons){
        }

        U64 idat_length;
        memcpy(&idat_length, temp -> buf + 33, 4);
        idat_length = ntohl(idat_length);

        U8 idat_buff[idat_length];

        memcpy(&idat_buff, temp -> buf + 41, idat_length);

        U64 out_length = 0;
        int ret= 0;

        pthread_mutex_lock(&(g -> idat_mutex));

        ret = mem_inf((idat -> buf + idat -> size), &out_length, idat_buff, idat_length);

        if (ret != 0) { /* failure */
            fprintf(stderr,"mem_inf failed. ret = %d.\n", ret);
        }

        idat -> size += out_length;
        (g -> next_cons)++;

        pthread_mutex_unlock(&(g -> idat_mutex));

        sem_post(&(g -> empty));
    }
}

void concat_image(idat_chunk* idat_pointer, queue* q) {
    int height = ntohl(300);

    U8 compressed_data_buff[(400*4 + 1)* 300];
    U64 total_compressed_length = 0;

    int new_ret = 0;

    new_ret = mem_def(compressed_data_buff, &total_compressed_length, idat_pointer -> buf, idat_pointer -> size, Z_DEFAULT_COMPRESSION);

    if (new_ret != 0) { /* failure */
        fprintf(stderr,"mem_inf failed. ret = %d.\n", new_ret);
    }

    U64 total_compressed_length_out = htonl(total_compressed_length);

    unsigned char header[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char ihdr_buffer[25];

    recv_chunk* temp = q -> buf;

    memcpy(&ihdr_buffer, temp -> buf + 8, 25);

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
}

int main( int argc, char** argv )
{

    int buf_size = atoi(argv[1]);
	int num_prod = atoi(argv[2]);
	int num_con = atoi(argv[3]);
	int sleep_time = atoi(argv[4]);
	int image_num = atoi(argv[5]);

	// int buf_size = 5;
	// int num_prod = 5;
	// int num_con = 5;
	// int sleep_time = 10;
	// int image_num = 1;

    int queue_id = shmget(IPC_PRIVATE, sizeof(queue) + sizeof(recv_chunk)*buf_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    void* temp_pointer = shmat(queue_id, NULL, 0);
    queue* queue_pointer = (queue*) temp_pointer;
    queue_init(queue_pointer, temp_pointer, buf_size);

	int global_id = shmget(IPC_PRIVATE, sizeof(g_vars), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	void* global_temp = shmat(global_id, NULL, 0);
	g_vars* var_pointer = (g_vars*) global_temp;
	global_init(var_pointer, image_num, buf_size);

    int idat_id = shmget(IPC_PRIVATE, sizeof(idat_chunk) + (400*4 + 1)* 300 , IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    void* idat_temp = shmat(idat_id, NULL, 0);
    idat_chunk* idat_buf = (idat_chunk*) idat_temp;
    idat_init(idat_buf, idat_temp);

	pid_t main_pid;
	pid_t cpids[num_con + num_prod];
	int state;
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

	// producers
	for( int i = 0; i < num_prod; i++) {

		main_pid = fork();

		if ( main_pid > 0 ) {
			cpids[i] = main_pid;
		}
		else if ( main_pid == 0 ) {
            producer(queue_pointer, var_pointer);

            shmdt(temp_pointer);
            shmdt(global_temp);
            shmdt(idat_temp);
			return 0;
		}
		else {
			perror("fork");
			exit(3);
		}
	}

	// consumers
	for( int i = num_prod; i < num_con + num_prod; i++ ) {

		main_pid = fork();

		if ( main_pid > 0 ) {
			cpids[i] = main_pid;
		}
		else if ( main_pid == 0 ) {
            consumer(queue_pointer, var_pointer, idat_buf, sleep_time);

            shmdt(temp_pointer);
            shmdt(global_temp);
            shmdt(idat_temp);
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
            }
		}

        concat_image(idat_buf, queue_pointer);

        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;

        pthread_mutex_destroy(&(var_pointer -> p_count_mutex));
        pthread_mutex_destroy(&(var_pointer -> c_count_mutex));
        pthread_mutex_destroy(&(var_pointer -> queue_mutex));
        pthread_mutex_destroy(&(var_pointer -> idat_mutex));
        sem_destroy(&(var_pointer -> empty));
        sem_destroy(&(var_pointer -> full));

        shmdt(temp_pointer);
        shmctl(queue_id, IPC_RMID, NULL);

        shmdt(global_temp);
        shmctl(global_id, IPC_RMID, NULL);

        shmdt(idat_temp);
        shmctl(idat_id, IPC_RMID, NULL);

        printf("paster2 execution time: %.6lf seconds\n",  times[1] - times[0]);
	}

	return 0;
}
