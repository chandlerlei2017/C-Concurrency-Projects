#include "linked_list.h"
#include "findpng3.h"

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

int main( int argc, char** argv )
{
  //Get arguments
  int l_file = 0;
  int t = 1;
  char v[256];
  int m = 50;
  char* base_url = malloc(256);
  strcpy(v, "");


  if (get_args(argc, argv, &t, &l_file, &m, v, base_url) == -1) {
    return -1;
  }

  //printf("arguments were: t: %d, m: %d, v: %s, base_url: %s \n", t, m, v, base_url);

}
