#include "findpng2.h"

typedef struct thread_args              /* thread input parameters struct */
{
  linked_list* url_frontier;
} thread_args;

// Thread function to process the urls
void *process_url(void *arg) {
  thread_args *p_in = arg;
  linked_list* url_frontier = p_in -> url_frontier;

  while (url_frontier -> head != NULL) {
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    char* curr_url = pop(p_in -> url_frontier);

    printf("current url: %s\n", curr_url);
    print_list(url_frontier);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = easy_handle_init(&recv_buf, curr_url);

    if ( curl_handle == NULL ) {
      fprintf(stderr, "Curl initialization failed. Exiting...\n");
      curl_global_cleanup();
      abort();
    }

    res = curl_easy_perform(curl_handle);

    if( res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      cleanup(curl_handle, &recv_buf);
    } else {
	    printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);

      process_data(curl_handle, &recv_buf, url_frontier);
      printf("\n");

      cleanup(curl_handle, &recv_buf);
      free(curr_url);
    }
  }
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

  //printf("t: %d, m: %d, v: %s, %s \n", t, m, v, e.key);

  // Initialize the hashset

  hcreate(100000);

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

  // cleanup

  free(p_tids);
  hdestroy();
  list_cleanup(url_frontier);
  free(url_frontier);

  return 0;
}
