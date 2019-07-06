#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <search.h>
#include <pthread.h>
#include <getopt.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#include "linked_list.h"

#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9

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

typedef struct thread_args              /* thread input parameters struct */
{
    linked_list* url_frontier;
} thread_args;


// Thread function to process the urls
void *process_url(void *arg) {
  thread_args *p_in = arg;

}

int main( int argc, char** argv )
{
  int c;
  int t = 1;
  int m = 5;
  char v[256];
  char base_url[256];
  int count = 0;

  linked_list* url_frontier = malloc(sizeof(linked_list));
  init(url_frontier);

  ENTRY e, *ep;

  char *str = "option requires an argument";
  strcpy(v, "log_file.txt");

  // get arguments

  while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
    switch (c) {
    case 't':
    t = strtoul(optarg, NULL, 10);
    if (t <= 0) {
      fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
      return -1;
    }
    break;
    case 'm':
    m = strtoul(optarg, NULL, 10);
    if (m <= 0) {
      fprintf(stderr, "%s: %s > 0 -- 'n'\n", argv[0], str);
      return -1;
    }
    break;
    case 'v':
    memcpy(v, optarg, strlen(optarg) + 1);
    break;
    default:
      return -1;
    }

    count += 1;
  }

  // Initialize the hashset

  hcreate(1000);

  memcpy(base_url, argv[2*count + 1], strlen(argv[2*count + 1]) + 1);

  e.key = base_url;
  e.data = NULL;

  ep = hsearch(e, ENTER);

  if (ep == NULL) {
      fprintf(stderr, "Hash table entry failed\n");
      exit(EXIT_FAILURE);
  }

  // push base_url into url frontier
  push(url_frontier, e.key);

  // create threads
  pthread_t *p_tids = malloc(sizeof(pthread_t) * t);

  thread_args in_params;
  in_params.url_frontier = url_frontier;

  for (int i = 0; i < t; i++) {
    pthread_create(p_tids + i, NULL, process_url, &in_params);
  }

  // wait for threads
  for (int i=0; i < t; i++) {
      pthread_join(p_tids[i], NULL);
      printf("Thread ID %lu joined.\n", p_tids[i]);
  }

  free(p_tids);

  //printf("t: %d, m: %d, v: %s, %s \n", t, m, v, e.key);

  // destroy the hashset
  hdestroy();
  return 0;
}
