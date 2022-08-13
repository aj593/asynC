#include "http_utility.h"

#include <string.h>
#include <ctype.h>

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
    for(int i = 0; i < 20 && num_str[i] != '\0'; i++){
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
        //TODO: need mutex lock here?
        prepend(http_data_list_ptr, new_request_data);
    }
}