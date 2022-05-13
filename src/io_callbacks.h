#ifndef IO_CALLBACKS
#define IO_CALLBACKS

#include <unistd.h>
#include <stdio.h>

#include "singly_linked_list.h"
#include "async_io.h"

//TODO: make code go into .c file but only array left here!

void open_cb_interm(event_node*);
void read_cb_interm(event_node*);
void write_cb_interm(event_node*);
void read_file_cb_interm(event_node*);
void write_file_cb_interm(event_node*);

#endif
