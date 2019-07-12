#include "linked_list.h"
#include "findpng2.h"

char v[256];
int png_count;
int m;
int t;
int first_flag;
int break_thread;
pthread_mutex_t ll_mutex;
pthread_mutex_t count_mutex;

int p_count;
pthread_mutex_t barrier_mutex;
pthread_cond_t cv;

char* pointers[1000*sizeof(char*)];
int pointer_count;

int find_http(char *fname, int size, int follow_relative_links, const char *base_url, linked_list* url_frontier);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier, char* curr_url);
void test_hash();
int insert_hash(char* str);

void barrier() {
  pthread_mutex_lock(&barrier_mutex);
  p_count++;
  if (p_count < t) {
    pthread_cond_wait(&cv, &barrier_mutex);
  } else {
    first_flag = 0;
    p_count = 0;
    pthread_cond_broadcast(&cv);
  }
  pthread_mutex_unlock(&barrier_mutex);
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

                  pthread_mutex_lock(&ll_mutex);

                  if(insert_hash(new_href) == 1) {
                    push(url_frontier, new_href);
                  }

                  pointers[pointer_count] = new_href;
                  pointer_count += 1;

                  pthread_mutex_unlock(&ll_mutex);

            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL;
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url, url_frontier);
    sprintf(fname, "./output_%d.html", pid);
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char *eurl = NULL;          /* effective URL */
    int flag = 0;

    pthread_mutex_lock(&count_mutex);

    if( png_count < m) {
      flag = 1;
      png_count +=1;
    }

    pthread_mutex_unlock(&count_mutex);

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);

    if (flag == 1) {
      FILE* fp = fopen("png_urls.txt", "a+");
      fprintf(fp, "%s\n", eurl);

      fclose(fp);
    }

    return 0;
}
/**
 * @brief process the download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data.
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, linked_list* url_frontier, char* curr_url)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;
    char *eff_url = NULL;

    FILE* fp = fopen(v, "a+");

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eff_url);

    ENTRY e;
    e.key = eff_url;

    if (hsearch(e, FIND) == NULL) {
      fprintf(fp, "%s\n", eff_url);
    }

    if ( res == CURLE_OK ) {
    }

    if ( response_code >= 200 && response_code < 300) {
    	fprintf(fp, "%s\n", curr_url);
    }
    else if ( response_code >= 400 && response_code < 500) {
    	fprintf(stderr, "Error.\n");
      return 1;
    }
    else if ( response_code >= 500 && response_code < 600) {
      fprintf(fp, "%s\n", curr_url);
      return 1;
    }

    fclose(fp);

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
                //printf("real: %X, expected: %X \n", buffer[i], expected[i]);
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
    } else {
        sprintf(fname, "./output_%d", pid);
    }

    return 0;
}

void test_hash(){
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/sub1/index.html");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/sub1/index.html");
  insert_hash("http://ece252-1.uwaterloo.ca:2540/image?q=gAAAAABdHkoqPlVGPQPMpFG8Vjvrodu_pZqXMC4jGRiLcYwY6MkhhFG1m5a3x5ZYDOiLGFmz8FTbq3sAva7QKiXY5YNIxrBdxg==");
  insert_hash("http://ece252-3.uwaterloo.ca:2540/image?q=gAAAAABdHkoqOKR-cFRCkiCUBEMAAAWfDvBFlRisL9ysLWHYHbcQbn1b28PV_uHBZ0gJf5bvzrnf1HNXxB6KRlAVETwTIqBH2Q==");
  insert_hash("http://ece252-1.uwaterloo.ca:2531/image?img=1&part=0");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/imgs/Disguise.png");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/imgs/crawler_arch.jpg");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/imgs/para.jpg");
  insert_hash("http://ece252-1.uwaterloo.ca/lab4");
  insert_hash("http://ece252-1.uwaterloo.ca:2540/image?q=gAAAAABdHkoqPlVGPQPMpFG8Vjvrodu_pZqXMC4jGRiLcYwY6MkhhFG1m5a3x5ZYDOiLGFmz8FTbq3sAva7QKiXY5YNIxrBdxg==");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/imgs/cpp.jpg");
  insert_hash("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/#top");
}

typedef struct thread_args              /* thread input parameters struct */
{
  linked_list* url_frontier;
} thread_args;

// Thread function to process the urls
void *process_url(void *arg) {
  thread_args *p_in = arg;
  linked_list* url_frontier = p_in -> url_frontier;

  while (1) {
    barrier();

    pthread_mutex_lock(&count_mutex);

    if (png_count >= m) {
      pthread_mutex_unlock(&count_mutex);
      break;
    }

    pthread_mutex_unlock(&count_mutex);

    pthread_mutex_lock(&ll_mutex);

    if (url_frontier -> head == NULL && first_flag == 0) {
      break_thread = 1;
    }

    first_flag = 1;

    if (break_thread == 1) {
      pthread_mutex_unlock(&ll_mutex);
      break;
    }

    if (url_frontier -> head == NULL) {
      pthread_mutex_unlock(&ll_mutex);
      continue;
    }

    char* curr_url = pop(url_frontier);

    pthread_mutex_unlock(&ll_mutex);

    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;


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
      process_data(curl_handle, &recv_buf, url_frontier, curr_url);

      cleanup(curl_handle, &recv_buf);
    }
    free(curr_url);
  }
  return NULL;
}

int main( int argc, char** argv )
{
  int c;
  t = 1;
  m = 50;
  char* base_url = malloc(256);
  int count = 0;
  p_count = 0;
  first_flag = 0;
  break_thread = 0;
  pointer_count = 0;

  pthread_mutex_init (&ll_mutex , NULL);
  pthread_mutex_init (&count_mutex , NULL);

  pthread_mutex_init (&barrier_mutex , NULL);
  pthread_cond_init(&cv, NULL);

  linked_list* url_frontier = malloc(sizeof(linked_list));
  init(url_frontier);

  char *str = "option requires an argument";
  strcpy(v, "log.txt");

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

  //printf("t: %d, m: %d, v: %s, %s \n", t, m, v, base_url);

  // Initialize the hashset

  hcreate(1000);

  memcpy(base_url, argv[2*count + 1], strlen(argv[2*count + 1]) + 1);

  if(insert_hash(base_url) == 1) {
    push(url_frontier, base_url);
  }

  // create threads
  pthread_t *p_tids = malloc(sizeof(pthread_t) * t);

  thread_args in_params;
  in_params.url_frontier = url_frontier;

  FILE* fp = fopen("png_urls.txt", "w");
  fclose(fp);

  fp = fopen(v, "w");
  fclose(fp);

  png_count = 0;

  for (int i = 0; i < t; i++) {
    pthread_create(p_tids + i, NULL, process_url, &in_params);
  }

  // wait for threads
  for (int i=0; i < t; i++) {
      pthread_join(p_tids[i], NULL);
      printf("Thread ID %lu joined.\n", p_tids[i]);
  }

  // cleanup
  for (int i = 0; i < pointer_count; i++) {
    free(pointers[i]);
  }

  pthread_mutex_destroy(&ll_mutex);
  pthread_mutex_destroy(&count_mutex);

  pthread_mutex_destroy(&barrier_mutex);
  pthread_cond_destroy(&cv);

  free(base_url);
  free(p_tids);
  hdestroy();
  list_cleanup(url_frontier);
  free(url_frontier);

  return 0;
}
