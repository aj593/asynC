#include "event_loop.h"

#include "../async_types/event_emitter.h"

#include "../async_lib/async_io/io_callbacks.h"
#include "../async_lib/async_child/child_callbacks.h"
#include "../async_lib/readstream/readstream_callbacks.h"

#include "../containers/hash_table.h"
#include "../containers/c_vector.h"

#include "event_routing.h"

//#include "../async_lib/async_io.h"
//#include "../async_lib/async_child.h"
//#include "async_lib/readstream.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

//TODO: should this go in .c or .h file for visibility?
linked_list event_queue; //singly linked list that keeps track of items and events that have yet to be fulfilled

//TODO: put this in a different file?
hash_table* subscriber_hash_table; //hash table that maps null-terminated strings to vectors of emitter items so we keep track of which emitters subscribed what events

//TODO: use is_linked_list_empty() instead? (but uses extra function call)
int is_event_queue_empty(){
    return event_queue.size == 0;
}

//TODO: error check this so user can error check it
void asynC_init(){
    linked_list_init(&event_queue); //TODO: error check singly_linked_list.c
    subscriber_hash_table = ht_create();
}

//TODO: add a mutex lock and make mutex calls to this when appending and removing items?
void enqueue_event(event_node* event_node){
    append(&event_queue, event_node);
}

//TODO: error check this?
void asynC_wait(){
    while(!is_event_queue_empty()){
        event_node* curr = event_queue.head;
        while(curr != event_queue.tail){
            //TODO: is this if-else statement correct? curr = curr->next?
            int internal_event_index = 0; //TODO: initialize this value?
            if(is_event_completed(curr->next, &internal_event_index)){
                event_node* exec_node = remove_next(&event_queue, curr);
                //TODO: check if exec_cb is NULL?
                void(*exec_cb)(event_node*) = exec_cb_array[exec_node->event_index][internal_event_index];
                exec_cb(exec_node);

                destroy_event_node(exec_node);
            }
            else{
                curr = curr->next;
            }
        }
    }

    //TODO: destroy all vectors in hash_table too and clean up other stuff
    linked_list_destroy(&event_queue);
    ht_destroy(subscriber_hash_table);
}

//TODO: implement something like process.nextTick() here?