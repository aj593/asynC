#ifndef FS_CALLBACKS
#define FS_CALLBACKS

#include "../containers/singly_linked_list.h"

void fs_open_interm(event_node*);
void read_cb_interm(event_node* exec_node);

void fs_chmod_interm(event_node* exec_node);
void fs_chown_interm(event_node* exec_node);

#endif