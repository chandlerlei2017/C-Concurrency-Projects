#include "linked_list.h"
#include "findpng3.h"

int png_count;
int l_file;
char v[256];
int m;

char* pointers[1000*sizeof(char*)];
int pointer_count;
RECV_BUF** buffers;

typedef struct curl_info {
  int index;
  char* url;
} curl_info;

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, linked_list* url_frontier);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier, char* curr_url);

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
        fprintf(stderr, "Failed obtain Content-Type\n");
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

            FILE* fp;

            if (l_file == 1) {
              fp = fopen(v, "a+");
            }

            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
              char* new_href = (char* ) xmlStrdup(href);

              if(insert_hash(new_href) == 1) {
                push(url_frontier, new_href);
                if (l_file == 1) {
                  fprintf(fp, "%s\n", new_href);
                }
              }

              pointers[pointer_count] = new_href;
              pointer_count += 1;
            }
            xmlFree(href);

            if (l_file == 1) {
              fclose(fp);
            }
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

static void ch_init(CURLM *cm, char* url, int index)
{
  RECV_BUF* buf = malloc(sizeof(RECV_BUF));
  CURL *eh = easy_handle_init(buf, url);

  curl_info* pass = malloc(sizeof(curl_info));

  pass -> index = index;
  pass -> url = url;
  buffers[index] = buf;

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

  linked_list* url_frontier = malloc(sizeof(linked_list));
  init(url_frontier);


  if (get_args(argc, argv, &t, &l_file, &m, v, base_url) == -1) {
    return -1;
  }

  buffers = malloc(sizeof(RECV_BUF*) * t);

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
  int still_running = 0;
  int msgs_left = 0;
  int http_status_code;
  curl_info* info;
  void* temp = NULL;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  cm = curl_multi_init();

  while (png_count < m && url_frontier -> head != NULL) {
    int curr = 0;

    while(url_frontier -> head != NULL && curr < t) {
      char* curr_url = pop(url_frontier);
      ch_init(cm, curr_url, curr);
      curr += 1;
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

        return_code = msg->data.result;
        if(return_code!=CURLE_OK) {
          //fprintf(stderr, "CURL error code: %d\n", msg->data.result);
          continue;
        }

        // Get HTTP status code
        http_status_code=0;

        curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
        curl_easy_getinfo(eh, CURLINFO_PRIVATE, &temp);

        info = (curl_info*) temp;

        if(http_status_code==200) {
          //printf("200 OK for %s\n", info -> url);
        } else {
          //fprintf(stderr, "GET of %s returned http status code %d\n", info -> url, http_status_code);
        }

        process_data(eh, buffers[info -> index], url_frontier, info -> url);

        curl_multi_remove_handle(cm, eh);
        cleanup(eh, buffers[info -> index]);
        free(buffers[info -> index]);
        free(info -> url);
        free(info);
      }
      else {
        //fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
      }
    }
  }

  for (int i = 0; i < pointer_count; i++) {
    free(pointers[i]);
  }

  curl_multi_cleanup(cm);
  curl_global_cleanup();
  free(url_frontier);
  free(buffers);

  return 0;
}
