#include "http_utility.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HEADER_BUFF_SIZE 50

#define CRLF_STR "\r\n"
#define CRLF_LEN 2

//TODO: use library function instead?
int str_to_num(char* num_str){
    int content_len_num = 0;
    int curr_index = 0;
    while(num_str[curr_index] != '\0'){
        int curr_digit = num_str[curr_index] - '0';
        content_len_num = (content_len_num * 10) + curr_digit;

        curr_index++;
    }

    return content_len_num;
}

int is_number_str(char* num_str){
    for(int i = 0; /*i < 20 &&*/ num_str[i] != '\0'; i++){
        if(!isdigit(num_str[i])){
            return 0;
        }
    }

    return 1;
}

void parse_http(
    buffer* buffer_to_parse, 
    char** start_line_first_token, 
    char** start_line_second_token,
    char** start_line_third_token,
    hash_table* header_table,
    size_t* content_length_ptr,
    linked_list* http_data_list_ptr
){
    char* header_string = (char*)get_internal_buffer(buffer_to_parse);
    //int buffer_capacity = get_buffer_capacity((*info_to_parse)->http_header_data);
    
    char* rest_of_str = header_string;
    char* curr_line_token = strtok_r(rest_of_str, "\r", &rest_of_str);
    char* token_ptr = curr_line_token;
    //TODO: get method, url, and http version in while-loop instead?
    char* curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    *start_line_first_token = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    *start_line_second_token = curr_first_line_token;

    curr_first_line_token = strtok_r(token_ptr, " ", &token_ptr);
    *start_line_third_token = curr_first_line_token;

    curr_line_token = strtok_r(rest_of_str, "\r\n", &rest_of_str);
    while(curr_line_token != NULL && strncmp("\r", curr_line_token, 10) != 0){
        char* curr_key_token = strtok_r(curr_line_token, ": ", &curr_line_token);
        char* curr_val_token = strtok_r(curr_line_token, "\r", &curr_line_token);
        while(curr_val_token != NULL && *curr_val_token == ' '){
            curr_val_token++;
        }

        ht_set(
            header_table,
            curr_key_token,
            curr_val_token
        );

        curr_line_token = strtok_r(rest_of_str, "\n", &rest_of_str);
    }

    char* content_length_num_str = ht_get(header_table, "Content-Length");
    if(content_length_num_str != NULL && is_number_str(content_length_num_str)){
        *content_length_ptr = str_to_num(content_length_num_str);
    }
    
    //copy start of buffer into data buffer
    size_t start_of_body_len = strnlen(rest_of_str, get_buffer_capacity(buffer_to_parse));
    if(start_of_body_len > 0){
        event_node* new_request_data = create_event_node(sizeof(buffer_item), NULL, NULL);
        buffer_item* buff_item_ptr = (buffer_item*)new_request_data->data_ptr;
        buff_item_ptr->buffer_array = buffer_from_array(rest_of_str, start_of_body_len);
        //TODO: need mutex lock here
        prepend(http_data_list_ptr, new_request_data);
    }
}

buffer* get_http_buffer(hash_table* header_table_ptr, int* request_header_length){
    hti request_header_iterator = ht_iterator(header_table_ptr);
    while(ht_next(&request_header_iterator)){
        int key_len = strlen(request_header_iterator.key);
        int val_len = strlen((char*)request_header_iterator.value);
        *request_header_length += key_len + val_len + 4;
    }

    buffer* http_request_header_buffer = create_buffer(*request_header_length, 1);
    return http_request_header_buffer;
}

void async_http_set_header(
    char* header_key, 
    char* header_val, 
    char** string_buffer_ptr,
    size_t* buffer_len_ptr,
    size_t* buffer_cap_ptr,
    hash_table* header_table
){
    size_t key_len = strlen(header_key);
    size_t value_len = strlen(header_val);
    size_t new_header_len = key_len + value_len + 2;
    size_t new_buffer_len = *buffer_len_ptr + new_header_len;
    //TODO: make sure indexing with capacity and lengths work?
    if(new_buffer_len > *buffer_cap_ptr){
        size_t new_buffer_capacity = *buffer_cap_ptr + HEADER_BUFF_SIZE;
        while(new_buffer_len > new_buffer_capacity){
            new_buffer_capacity += HEADER_BUFF_SIZE;
        }

        char* new_header_str = (char*)realloc(*string_buffer_ptr, *buffer_cap_ptr + HEADER_BUFF_SIZE);
        if(new_header_str == NULL){
            //TODO: error handling here
            return;
        }

        *string_buffer_ptr = new_header_str;
        *buffer_cap_ptr = new_buffer_capacity;
    }

    char* string_buffer = *string_buffer_ptr;

    strncpy(
        string_buffer + *buffer_len_ptr, 
        header_key,
        key_len
    );

    size_t key_offset = *buffer_len_ptr;

    *buffer_len_ptr += key_len;

    string_buffer[*buffer_len_ptr] = '\0';
    *buffer_len_ptr = *buffer_len_ptr + 1;

    strncpy(
        string_buffer + *buffer_len_ptr,
        header_val,
        value_len
    );

    size_t val_offset = *buffer_len_ptr;

    *buffer_len_ptr += value_len;

    string_buffer[*buffer_len_ptr] = '\0';
    *buffer_len_ptr = *buffer_len_ptr + 1;

    ht_set(
        header_table, 
        string_buffer + key_offset, 
        string_buffer + val_offset
    );
}

void copy_start_line(
    char** buffer_internal_array, 
    char* first_str,
    int first_str_len,
    char* second_str,
    int second_str_len,
    char* third_str,
    int third_str_len
){
    //copy start line into buffer
    memcpy(*buffer_internal_array, first_str, first_str_len);
    *buffer_internal_array += first_str_len;

    memcpy(*buffer_internal_array, " ", 1);
    (*buffer_internal_array)++;

    memcpy(*buffer_internal_array, second_str, second_str_len);
    *buffer_internal_array += second_str_len;

    memcpy(*buffer_internal_array, " ", 1);
    (*buffer_internal_array)++;

    memcpy(*buffer_internal_array, third_str, third_str_len);
    *buffer_internal_array += third_str_len;
    
    int CRLF_len = 2;
    memcpy(*buffer_internal_array, "\r\n", CRLF_len);
    *buffer_internal_array += CRLF_len;
}

void copy_single_header_entry(char** destination_buffer, const char* key, char* value){
    int curr_header_key_len = strlen(key);
    memcpy(*destination_buffer, key, curr_header_key_len);
    *destination_buffer += curr_header_key_len;

    int colon_space_len = 2;
    memcpy(*destination_buffer, ": ", colon_space_len);
    *destination_buffer += colon_space_len;

    int curr_header_val_len = strlen(value);
    memcpy(*destination_buffer, value, curr_header_val_len);
    *destination_buffer += curr_header_val_len;

    memcpy(*destination_buffer, CRLF_STR, CRLF_LEN);
    *destination_buffer += CRLF_LEN;
}

void copy_all_headers(char** destination_buffer, hash_table* header_table){
    hti header_writer_iterator = ht_iterator(header_table);
    while(ht_next(&header_writer_iterator)){
        copy_single_header_entry(
            destination_buffer, 
            header_writer_iterator.key,
            (char*)header_writer_iterator.value
        );
    }

    memcpy(*destination_buffer, CRLF_STR, CRLF_LEN);
    *destination_buffer += CRLF_LEN;
}

void http_buffer_check_for_double_CRLF(
    async_socket* read_socket,
    int* found_double_CRLF_ptr,
    buffer** base_buffer_dble_ptr,
    buffer* new_data_buffer,
    void data_handler_to_remove(async_socket*, buffer*, void*),
    void data_handler_to_add(async_socket*, buffer*, void*),
    void* arg
){
    int old_data_capacity = get_buffer_capacity(*base_buffer_dble_ptr);
    int new_data_capacity = get_buffer_capacity(new_data_buffer);

    int char_buffer_check_length = old_data_capacity + new_data_capacity + 1;

    char* old_data_internal_buffer = get_internal_buffer(*base_buffer_dble_ptr);
    char* new_data_internal_buffer = get_internal_buffer(new_data_buffer);

    char char_buffer_check_array[char_buffer_check_length];
    strncpy(char_buffer_check_array, old_data_internal_buffer, old_data_capacity);
    strncpy(char_buffer_check_array + old_data_capacity, new_data_internal_buffer, new_data_capacity);
    char_buffer_check_array[char_buffer_check_length - 1] = '\0';

    int num_buffers_to_concat = 2;
    buffer* buffer_array[] = {
        *base_buffer_dble_ptr,
        new_data_buffer
    };

    buffer* new_concatted_buffer = buffer_concat(buffer_array, num_buffers_to_concat);
    destroy_buffer(*base_buffer_dble_ptr);
    destroy_buffer(new_data_buffer);
    *base_buffer_dble_ptr = new_concatted_buffer;

    if(strstr(char_buffer_check_array, "\r\n\r\n") == NULL){
        return;
    }

    //TODO: remove "data_handler_to_remove" data handler here

    /*
    async_socket_on_data(
        read_socket,
        data_handler_to_add,
        arg,
        0,
        0
    );
    */

    *found_double_CRLF_ptr = 1;
}