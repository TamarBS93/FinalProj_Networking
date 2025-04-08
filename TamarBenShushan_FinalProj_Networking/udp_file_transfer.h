#ifndef MYFUNCTIONS_H
#define MYFUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

#define DATA_SIZE 994
#define BUFFER_SIZE 1024

#pragma pack(1)  // Disable padding
typedef struct Packet Packet;
struct Packet {
    /* op_id:
        Sending packets:
        -1: download 0: delete x>0: upload
        Receiving packets:
        -1: error x>0: packet nuber
    */
    int op_id; 
    char file_name[26];
    char data[DATA_SIZE];
};
#pragma pack()  // Restore default packing


void clear_input_buffer(){
    char ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
        // Keep reading characters until we encounter a newline or EOF
    }
}

char* filename_in(){
    char *file_in = (char*)malloc(26 * sizeof(char));  // Dynamically allocate memory
    if (file_in == NULL) {
        printf("Memory allocation failed\n");
        return NULL;  // Return NULL if memory allocation fails
    }

    printf("File name: ");
    if (fgets(file_in, 26, stdin) == NULL) {
        printf("Invalid input\n");
        free(file_in);  // Free the allocated memory
        return NULL;
    }
    file_in[strcspn(file_in, "\n")] = '\0';  // Remove trailing newline
    // clear_input_buffer();
    puts("");
    return file_in;
}

#endif