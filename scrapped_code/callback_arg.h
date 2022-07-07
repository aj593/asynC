#ifndef CALLBACK_ARG
#define CALLBACK_ARG

typedef struct cb_arg callback_arg;

#include <stddef.h>

callback_arg* create_cb_arg(void* original_data, size_t arg_size);
void destroy_cb_arg(callback_arg* cb_arg);

void* get_arg_data(callback_arg* cb_arg);
size_t get_arg_size(callback_arg* cb_arg);

#endif