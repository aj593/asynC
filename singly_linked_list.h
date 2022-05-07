#ifndef SINGLY_LINKED_LIST
#define SINGLY_LINKED_LIST

typedef void(*callback)(void* arg);

#include <aio.h>

typedef struct event_node{
    callback callback;
    void* callback_arg;
    struct aiocb aio_block;
    struct event_node *next;   //next pointer in linked list
} event_node;

typedef struct linked_list{
    event_node *head;
    event_node *tail;
    int size;
} linked_list;

void initialize_linked_list(linked_list* list);
void destroy_linked_list(linked_list* list);
int is_linked_list_empty(linked_list* list);

void add_next(linked_list* list, event_node* curr, event_node* new_node);
void prepend(linked_list* list, event_node* new_first);
void append(linked_list* list, event_node* new_tail);

event_node* remove_next(linked_list* list, event_node* current);
event_node* remove_first(linked_list* list);
event_node* remove_last(linked_list* list);

#endif