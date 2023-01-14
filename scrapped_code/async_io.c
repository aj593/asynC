//#include "async_io.h"
//#include "../util/linked_list.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#define IO_EVENT_INDEX 0    //index number so we know which function pointer array in two-dimensional function pointer array to look at

//the following are indexes in the array of function pointers that correspond to an I/O operation type so we know how to handle and execute the callback
//#define OPEN_INDEX 0        //used for async_open()
#define READ_INDEX 0        //used for async_read()
#define WRITE_INDEX 1       //used for async_write()
#define READ_FILE_INDEX 2   //used for read_file()
#define WRITE_FILE_INDEX 3  //used for write_file()

//TODO: make sure to close file descriptors if user doesn't have direct access to them when operations are being done, like in read_file and write_file

//TODO: make it so offset in file changes whenever reading or writing
//TODO: handle case if callback is NULL?

//initialize the I/O event we're going to perform by creating an event node for it, which will later be enqueued into the event queue
//this will be used repeatedly in our different asynchronous I/O operations
/*event_node* io_event_init(int io_index, async_byte_buffer* io_buffer, void(*io_callback_handler)(event_node*), callback_arg* cb_arg){
    //create the event node, and also allocate a block of data for its respective piece of data, the size of an async_io struct
    event_node* new_io_event = create_event_node(IO_EVENT_INDEX, sizeof(async_io));
    
    new_io_event->callback_handler = io_callback_handler;

    //obtain our pointer to our async_io struct so we can assign values to it in the next few lines
    async_io* io_block = &new_io_event->data_used.io_info; //(async_io*)new_io_event->event_data;

    //assign index for I/O operation, buffer that will have contents filled, and the callback argument passed into the callback
    io_block->io_index = io_index;
    io_block->buff_ptr = io_buffer;
    //io_block->io_callback = callback;
    io_block->callback_arg = cb_arg;

    return new_io_event; //return created event node, will be enqueued into event queue by caller
}*/

//where we will make our asynchronous I/O request with our aiocb block, aio operation, and other needed parameters
void make_aio_request(struct aiocb* aio_ptr, int file_descriptor, void* buff_for_aio, int num_bytes, int offset, aio_op async_op){
    //set the aiocb block's file descriptor, buffer, number of bytes to do for I/O, and the offset in the file based on passed-in args
    aio_ptr->aio_fildes = file_descriptor;
    aio_ptr->aio_buf = buff_for_aio;
    aio_ptr->aio_nbytes = num_bytes;
    aio_ptr->aio_offset = offset;

    //execute the operation shown by the aio_op function pointer and get its result value
    int result = async_op(aio_ptr);
    //TODO: is this proper error checking? may need extra parameter in this function to show how to handle error, or int return type?
    //check result value and error check accordingly
    if(result == -1){
        //new_read_event->callback_arg = NULL;
        //perror("aio_read: ");
    }
}

//asynchronously open file with provided file name, flags, mode, and callback information
/*void async_open(char* filename, int flags, int mode, open_callback open_cb, callback_arg* cb_arg){
    //create event node and initialize the async_io struct with the passed-in params
    event_node* new_open_event = 
        io_event_init(
            OPEN_INDEX, //we're using the intermediate function that handles the async_open's callback
            NULL,       //this operation doesn't do any reading or writing, we're only opening a file, so no async_byte_buffer* needed
            //open_cb, 
            cb_arg      //assign callback argument from passed-in cb_arg
        );

    //get io_block data and assign callback function and new open file descriptor
    async_io* io_open_block = (async_io*)new_open_event->event_data;
    io_open_block->io_callback.open_cb = open_cb;
    //open file in non-blocking manner
    io_open_block->aio_block.aio_fildes = open(filename, flags | O_NONBLOCK, mode); //TODO: make this actually async somehow

    //enqueue event node onto event queue
    enqueue_event(new_open_event);
}

//TODO: throw exception or error handle if num_bytes_to_read > read_buff_ptr capacity, or make it so read is automatically minimum of of two?
//asynchronously read the specified number of bytes into file and fill it into async_byte_buffer*'s data pointer, and also assign callback information for it
void async_read(int read_fd, async_byte_buffer* read_buff_ptr, int num_bytes_to_read, read_callback read_cb, callback_arg* cb_arg){
    //create event node and assign it proper fields in its async_io struct pointer
    event_node* new_read_event = 
        io_event_init(
            READ_INDEX,     //we're using the intermediate function that handles the async_read's callback
            read_buff_ptr,  //async_byte_buffer* that will have its data pointer filled with bytes
            read_cb_interm, 
            cb_arg          //callback argument from passed-in cb_arg
        );

    //get async_io struct and assign callback to it
    async_io* io_read_block = (async_io*)new_read_event->event_data;
    io_read_block->io_callback.read_cb = read_cb;

    //make aio_request to do aio_read() operation
    make_aio_request(
        &io_read_block->aio_block,                                          //use the aiocb from our async_io struct
        read_fd,                                                            //use the file descriptor passed-in for reading
        get_internal_buffer(read_buff_ptr),                                 //void* data we fill buffer with from aio_read
        num_bytes_to_read,                                                  //number of bytes to read from file, TODO:make min(num_bytes_to_read, buffer.capacity)?
        lseek(read_fd, num_bytes_to_read, SEEK_CUR) - num_bytes_to_read,                                         //offset from which we start reading file
        aio_read                                                            //function pointer for our aio_read() operation
    );

    //increment file offset we want to read from in case we call async_read() again
    io_read_block->file_offset += num_bytes_to_read;

    //enqueue event node onto event queue
    enqueue_event(new_read_event);
}

//TODO: check if this works
//TODO: throw exception or error handle if num_bytes_to_write > read_buff_ptr capacity?
//asynchronously write the specified number of bytes from the async_byte_buffer* to the file given by the file descriptor, and later executes callback with provided argument
void async_write(int write_fd, async_byte_buffer* write_buff_ptr, int num_bytes_to_write, write_callback write_cb, callback_arg* cb_arg){
    //make new event node for this async write and assign it proper index, passed-in buffer, and callback argument
    event_node* new_write_event = 
        io_event_init(
            WRITE_INDEX,    //write index so we know which function pointer intermediate callback handler calls
            write_buff_ptr, //the buffer that will get its bytes copied from for our write operation
            write_cb_interm,
            cb_arg          //callback argument passed into callback
        );

    //get our async_io struct pointer and assign it its callback
    async_io* io_write_block = (async_io*)new_write_event->event_data;
    io_write_block->io_callback.write_cb = write_cb;

    //make aio request to write to file of the passed-in file descriptor, void* data buffer, starting from passed-in offset
    make_aio_request(
        &io_write_block->aio_block,                                             //aio block we assign field values into
        write_fd,                                                               //file descriptor we write into
        get_internal_buffer(write_buff_ptr),                                    //void* data buffer we copy bytes from into file
        num_bytes_to_write,                                                     //number of bytes to write to file
        lseek(write_fd, num_bytes_to_write, SEEK_CUR) - num_bytes_to_write,
        //io_write_block->file_offset,                                            //file offset we start writing from
        aio_write                                                               //aio_op function pointer we call in make_aio_request()
    );

    //increment file offset we want to read from in case we call async_read() again
    io_write_block->file_offset += num_bytes_to_write;

    //enqueue event node onto event queue
    enqueue_event(new_write_event);
}

//TODO: error check return values
//TODO: need int in callback params?
//asynchronously read a whole file and place its byte contents into a buffer
void read_file(char* file_name, readfile_callback rf_callback, callback_arg* cb_arg){
    //open file in non-blocking mode
    int read_fd = open(file_name, O_RDONLY | O_NONBLOCK); //TODO: need NONBLOCK flag here?
    //declare and assign stats from file so we can get the file size
    struct stat file_stats;
    int return_code = fstat(read_fd, &file_stats); //TODO: make this stat call async somehow?
    int file_size = file_stats.st_size;

    //create new event node and assign it proper index, a new buffer of the file's size, and callback argument
    event_node* new_readfile_event = 
        io_event_init(
            READ_FILE_INDEX,            //we use the index corresponding to the intermediate callback handler's function in the function pointer array
            create_buffer(file_size, sizeof(char)),   //buffer of size "file_size"
            read_file_cb_interm, 
            cb_arg                      //callback argument that will be passed into callback
        );

    //get async_io struct from new event node and assign it proper callback
    async_io* readfile_data = &new_readfile_event->data_used.io_info; //(async_io*)new_readfile_event->event_data;
    readfile_data->io_callback.rf_cb = rf_callback;

    //make aio request to read file with new file descriptor and new buffer
    make_aio_request(
        &readfile_data->aio_block,                      //aiocb we're using to make aio request
        read_fd,                                        //file descriptor we're reading with
        get_internal_buffer(readfile_data->buff_ptr),   //void* data we fill in buffer
        file_size,                                      //number of bytes we read from file
        0,                                              //starting offset from where we read in file
        aio_read                                        //aio function pointer we execute in make_aio_request to read from file
    );

    //enqueue event node onto event queue
    enqueue_event(new_readfile_event);
}

//TODO: make file creation async?
//TODO: throw exception or error handle if num_bytes_to_write > read_buff_ptr capacity?
//asynchronously write to a file with a async_byte_buffer* and specify the number of bytes, also allow options for flags and the mode, will execute callback with callback arg later
void write_file(char* file_name, async_byte_buffer* write_buff, int num_bytes_to_write, int mode, int flags, writefile_callback wf_cb, callback_arg* cb_arg){
    //make new event node to write to file
    event_node* new_writefile_event = 
        io_event_init(
            WRITE_FILE_INDEX,   //integer index for function pointer in fcn ptr array that we call intermediate callback handler
            write_buff,         //buffer we write into file
            write_file_cb_interm,
            cb_arg              //callback argument passed into callback
        );

    //get async_io struct pointer and assign it its proper callback
    async_io* io_wf_block = (async_io*)new_writefile_event;
    io_wf_block->io_callback.wf_cb = wf_cb;

    //make aio request to write whole buffer to file
    make_aio_request(
        &io_wf_block->aio_block,            //aiocb pointer we're using to make aio request
        open(file_name, flags, mode),       //file descriptor we're using to write to file
        get_internal_buffer(write_buff),    //void* data in async_byte_buffer* write_buff that we copy bytes from into file
        num_bytes_to_write,                 //number of bytes to write from the buffer to the file
        0,                                  //starting offset for file for where we're writing
        aio_write                           //aio_op we're passing in as function pointer to make async request
    );

    //enqueue event node onto event queue
    enqueue_event(new_writefile_event);
}*/