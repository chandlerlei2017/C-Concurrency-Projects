#include "linked_list.h"
#include "findpng3.h"

int png_count;
linked_list* url_frontier;

int get_args( int argc, char** argv, int* t, int* l_file, int* m, char* v, char* base_url) {
  int c;
  int count = 0;
  char *str = "option requires an argument";

  while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
    switch (c) {
    case 't':
    *t = strtoul(optarg, NULL, 10);
    if (*t <= 0) {
      fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
      return -1;
    }
    break;
    case 'm':
    *m = strtoul(optarg, NULL, 10);
    if (*m <= 0) {
      fprintf(stderr, "%s: %s > 0 -- 'n'\n", argv[0], str);
      return -1;
    }
    break;
    case 'v':
    memcpy(v, optarg, strlen(optarg) + 1);
    *l_file = 1;
    break;
    default:
      return -1;
    }

    count += 1;
  }
  if (2*count + 1 > argc - 1) {
    fprintf(stderr, "No base url provided \n");
    return -1;
  }

  memcpy(base_url, argv[2*count + 1], strlen(argv[2*count + 1]) + 1);
  return 0;
}

int insert_hash(char* str) {
  ENTRY e;
  e.key = str;

  if (hsearch(e, FIND) != NULL) {
    //printf("This is already in the Hashset: %s\n", str);
  }
  else {
    if(hsearch(e, ENTER) == NULL) {
      //printf("Unable to add to Hash: %s\n", str);
    }
    else {
      //printf("Added to Hash: %s\n", str);
      return 1;
    }
  }
  return 0;
}

int main( int argc, char** argv )
{
  // Get arguments
  int l_file = 0;
  int t = 1;
  char v[256];
  int m = 50;
  char base_url[256];
  png_count = 0;
  strcpy(v, "");

  url_frontier = malloc(sizeof(linked_list));
  init(url_frontier);


  if (get_args(argc, argv, &t, &l_file, &m, v, base_url) == -1) {
    return -1;
  }

  // Initialize Files

  FILE* fp = fopen("png_urls.txt", "w");
  fclose(fp);

  if (l_file == 1) {
    fp = fopen(v, "w");
    fclose(fp);
  }

  // Initialize Hash
  hcreate(1000);

  if(insert_hash(base_url) == 1) {
    push(url_frontier, base_url);

    if (l_file == 1) {
      fp = fopen(v, "a+");
      fprintf(fp, "%s\n", base_url);
      fclose(fp);
    }
  }

  // Initialize Curl Multi

  CURLM *cm=NULL;
  CURL *eh=NULL;
  CURLMsg *msg=NULL;
  CURLcode return_code=0;
  int still_running=0, i=0, msgs_left=0;
  int http_status_code;
  const char *szUrl;

  curl_global_init(CURL_GLOBAL_ALL);
  cm = curl_multi_init();

  curl_multi_cleanup(cm);
  return 0;
}
