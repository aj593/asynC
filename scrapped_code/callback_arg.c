//#include "callback_arg.h"

#include <string.h>
#include <stdlib.h>

typedef struct cb_arg {
    void* data;
    size_t size;
} callback_arg;

//TODO: is this proper error checking for this? DOUBLE CHECK THIS FUNCTION
callback_arg* create_cb_arg(void* original_data, size_t arg_size){
    //return NULL because we can't copy data from a NULL source (undefined behavior for memcpy)
    if(original_data == NULL){
        return NULL;
    }
    
    callback_arg* arg_ptr = (callback_arg*)malloc(sizeof(callback_arg));
    if(arg_ptr == NULL){
        return NULL;
    }

    arg_ptr->size = arg_size;
    arg_ptr->data = malloc(arg_size);

    if(arg_ptr->data == NULL){
        free(arg_ptr);
        return NULL;
    }

    memcpy(arg_ptr->data, original_data, arg_size);

    return arg_ptr;
}

void* get_arg_data(callback_arg* cb_arg){
    return cb_arg->data;
}

size_t get_arg_size(callback_arg* cb_arg){
    return cb_arg->size;
}

void destroy_cb_arg(callback_arg* cb_arg){
    free(cb_arg->data);
    free(cb_arg);
}