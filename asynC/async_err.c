#include "async_err.h"

void async_err_init(async_err* err_ptr, enum async_err_type err_type, int curr_errno){
    err_ptr->err_type = err_type;
    err_ptr->async_errno = curr_errno;
}