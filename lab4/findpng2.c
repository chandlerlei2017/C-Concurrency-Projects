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
  int i;

  linked_list* ll = malloc(sizeof(linked_list));
  init(ll);

  for (i = 0; i < 10; i++){
    char element[256];
    sprintf(element, "%d", i);

    insert(ll, element);
  }

  for (i = 0; i < 10; i++) {
    char* element = pop(ll);

    printf("%s\n", element);

    free(element);
  }



  free(ll);
  // Initialize the hashset
  hcreate(1000);



  // destroy the hashset
  hdestroy();
  return 0;
}