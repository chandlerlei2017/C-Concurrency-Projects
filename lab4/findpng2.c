#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <search.h>
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

// Thread function to process the urls
void *process_url(void *arg) {

}

int main( int argc, char** argv )
{
  int c;
  int t = 1;
  int m = 5;
  char v[256];
  char base_url[256];
  int count = 0;
  char *str = "option requires an argument";


  // Initialize the hashset
  hcreate(1000);

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

  memcpy(base_url, argv[2*count + 1], strlen(argv[2*count + 1]) + 1);

  //printf("t: %d, m: %d, v: %s, url: %s \n", t, m, v, base_url);



  // Code to test linked list implementation

  // linked_list* ll = malloc(sizeof(linked_list));
  // init(ll);

  // for (i = 0; i < 10; i++){
  //   char element[256];
  //   sprintf(element, "https://google.com/%d", i);

  //   insert(ll, element);
  // }

  // for (i = 0; i < 10; i++) {
  //   char* element = pop(ll);

  //   printf("%s\n", element);

  //   free(element);
  // }

  // cleanup(ll);
  // free(ll);


  // destroy the hashset
  hdestroy();
  return 0;
}
