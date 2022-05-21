#ifndef IO_CALLBACKS
#define IO_CALLBACKS

#include <unistd.h>
#include <stdio.h>

#include "../../containers/event_node.h"

void open_cb_interm(event_node*);
void read_cb_interm(event_node*);
void write_cb_interm(event_node*);
void read_file_cb_interm(event_node*);
void write_file_cb_interm(event_node*);

#endif
