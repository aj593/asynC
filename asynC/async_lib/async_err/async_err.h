#ifndef ASYNC_ERR_TYPE_H
#define ASYNC_ERR_TYPE_H

enum async_err_type {
    std_errno,
    async_errno
};

enum async_errno {
    ASYNC_ERRNO_NO_MEM
};

typedef struct async_err {
    enum async_err_type err_type;
    int async_errno;
    int std_errno;
} async_err;

void async_err_init(async_err* err_ptr, enum async_err_type, int curr_errno);

#endif