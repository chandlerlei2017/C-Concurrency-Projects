#include "linked_list.h"
#include "findpng3.h"

int png_count;
int l_file;
char v[256];
int m;

char* pointers[1000*sizeof(char*)];
int pointer_count;

typedef struct curl_info {
  RECV_BUF* buf;
  char* url;
} curl_info;

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, linked_list* url_frontier);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier, char* curr_url);

int insert_hash(char* str) {
  ENTRY e;
  e.key = str;

  if (hsearch(e, FIND) == NULL && hsearch(e, ENTER) != NULL) {
    return 1;
  }
  return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char *eurl = NULL;          /* effective URL */
    int flag = 0;

    if( png_count < m) {
      flag = 1;
      png_count +=1;
    }

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);

    if (flag == 1) {
      FILE* fp = fopen("png_urls.txt", "a+");
      fprintf(fp, "%s\n", eurl);

      fclose(fp);
    }

    return 0;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier)
{
    int follow_relative_link = 1;
    char *url = NULL;

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url, url_frontier);
    return 0;
}

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier, char* curr_url)
{
    CURLcode res;
    long response_code;
    char *eff_url = NULL;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eff_url);

    ENTRY e;
    e.key = eff_url;

    FILE* fp;

    if (l_file == 1) {
      fp = fopen(v, "a+");
    }

    if (hsearch(e, FIND) == NULL) {
      if (l_file == 1) {
        fprintf(fp, "%s\n", eff_url);
      }

      char* new_href = (char* ) strdup(eff_url);
      insert_hash(new_href);

      pointers[pointer_count] = new_href;
      pointer_count += 1;
    }

    else if(strcmp(curr_url, eff_url) != 0) {
      return 1;
    }

    if (l_file == 1) {
      fclose(fp);
    }

    if ( response_code >= 400 && response_code < 500) {
    	fprintf(stderr, "Error.\n");
      return 1;
    }
    else if ( response_code >= 500 && response_code < 600) {
      return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    } else {
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf, url_frontier);
    } else if ( strstr(ct, CT_PNG) ) {
        unsigned char expected[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        int real_png = 1;

        unsigned char buffer[8];
        memcpy(buffer, p_recv_buf -> buf, 8);

        for (int i = 0; i < 8; i++){
            if(buffer[i] != expected[i]) {
                real_png = 0;
                break;
            }
        }
        if (real_png == 1) {
          return process_png(curl_handle, p_recv_buf);
        }
        else{
          return 0;
        }
    }

    return 0;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, linked_list* url_frontier)
{
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);

    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
              char* new_href = (char* ) xmlStrdup(href);

              if(insert_hash(new_href) == 1) {
                push(url_frontier, new_href);
              }

              pointers[pointer_count] = new_href;
              pointer_count += 1;
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    return 0;
}

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

static void ch_init(CURLM *cm, char* url)
{
  RECV_BUF* buf = malloc(sizeof(RECV_BUF));
  CURL *eh = easy_handle_init(buf, url);

  curl_info* pass = malloc(sizeof(curl_info));

  pass -> buf = buf;
  pass -> url = url;

  curl_easy_setopt(eh, CURLOPT_PRIVATE, (void*) pass);

  if ( eh == NULL ) {
    fprintf(stderr, "Curl initialization failed. Exiting...\n");
    curl_global_cleanup();
    abort();
  }
  curl_multi_add_handle(cm, eh);
}

int main( int argc, char** argv )
{
  // Get arguments
  l_file = 0;
  int t = 1;
  m = 50;
  char base_url[256];
  png_count = 0;
  strcpy(v, "");

  double times[2];
  struct timeval tv;

  linked_list* url_frontier = malloc(sizeof(linked_list));
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
  }

  // Initialize Curl Multi
  CURLM *cm=NULL;
  CURL *eh=NULL;
  CURLMsg *msg=NULL;
  int still_running = 0;
  int msgs_left = 0;
  int http_status_code;
  curl_info* info;
  void* temp = NULL;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  cm = curl_multi_init();

  if (gettimeofday(&tv, NULL) != 0) {
      perror("gettimeofday");
      abort();
  }

  times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

  while (png_count < m && url_frontier -> head != NULL) {
    int curr = 0;

    FILE* fp;

    if (l_file == 1) {
      fp = fopen(v, "a+");
    }

    while(url_frontier -> head != NULL && curr < t) {
      char* curr_url = pop(url_frontier);
      ch_init(cm, curr_url);
      curr += 1;

      if (l_file == 1) {
        fprintf(fp, "%s\n", curr_url);
      }
    }

    if (l_file == 1) {
      fclose(fp);
    }

    curl_multi_perform(cm, &still_running);

    do {
      int numfds=0;
      int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
      if(res != CURLM_OK) {
          fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
          return EXIT_FAILURE;
      }
      curl_multi_perform(cm, &still_running);
    } while(still_running);

    while ((msg = curl_multi_info_read(cm, &msgs_left))) {
      if (msg->msg == CURLMSG_DONE) {
        eh = msg->easy_handle;

        // Get HTTP status code
        http_status_code=0;

        curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
        curl_easy_getinfo(eh, CURLINFO_PRIVATE, &temp);

        info = (curl_info*) temp;

        process_data(eh, info -> buf, url_frontier, info -> url);

        curl_multi_remove_handle(cm, eh);
        cleanup(eh, info -> buf);
        free(info -> buf);
        free(info -> url);
        free(info);
      }
    }
  }

  if (gettimeofday(&tv, NULL) != 0) {
    perror("gettimeofday");
    abort();
  }

  times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;

 printf("findpng3 execution time: %.6lf seconds\n",  times[1] - times[0]);

  for (int i = 0; i < pointer_count; i++) {
    free(pointers[i]);
  }

  curl_multi_cleanup(cm);
  curl_global_cleanup();
  free(url_frontier);

  return 0;
}
