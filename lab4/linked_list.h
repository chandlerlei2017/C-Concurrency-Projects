#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct node {
  char buffer[256];
  struct node* next;
} node;

typedef struct linked_list {
  node* head;
} linked_list;

void init(linked_list* ll) {
  ll -> head = NULL;
}

void push(linked_list* ll, char* buffer) {
  if (ll -> head == NULL) {
    ll -> head = (node*) malloc(sizeof(node));
    memcpy(ll -> head -> buffer, buffer, strlen(buffer) + 1);
    ll -> head -> next = NULL;
  }
  else {
    node* temp = (node*) malloc(sizeof(node));

    memcpy(temp -> buffer, buffer, strlen(buffer) + 1);
    temp -> next = ll -> head;
    ll -> head = temp;
  }
}

char* pop(linked_list* ll) {
  if(ll -> head == NULL) {
    printf("List size is zero, couldn't pop \n");
    exit(3);
  }
  else {
    node* temp = ll -> head;

    char* ret_val = malloc(strlen(temp -> buffer) + 1);
    memcpy(ret_val, temp -> buffer, strlen(temp -> buffer) + 1);
    ll -> head = temp -> next;

    free(temp);
    return ret_val;
  }
}

void list_cleanup(linked_list* ll) {
  node* it = ll -> head;
  while(it != NULL) {
    node* temp = it;
    it = it -> next;
    free(temp);
  }
  ll -> head = NULL;
}

void print_list(linked_list* ll) {
  node* it = ll -> head;

  printf("\nList Begins: \n");

  while(it != NULL) {
    printf("%s \n", it -> buffer);
    it = it -> next;
  }

  printf("List Ends \n");
}
