#include "udp_file_transfer.h"

int main()
{
    int sockfd;
    struct sockaddr_in server_addr;
    Packet send_packet;
    Packet received_packet;

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (inet_pton(AF_INET,SERVER_IP, &server_addr.sin_addr) <= 0){
        perror("invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int flag = 0;
    while(!flag){
        // Get input from the user
        printf("\nWhat whould you like to do?\n" 
                "R- Read (download)\n"
                "W- Write (upload)\n"
                "D- Delete\n"
                "Q- Quit\n");
        char op_char; 
        if (scanf("%c", &op_char) !=1) {
            printf("Invalid input:\n");
            return 1;
        }
        clear_input_buffer();

        FILE * fd;
        switch (op_char)
        {
        case 'q':
        case 'Q':
            flag = 1;
            break;
        case 'r':
        case 'R':
            //sending the request:
            send_packet.op_id = -1;
            strncpy(send_packet.file_name, filename_in(), sizeof(send_packet.file_name) - 1); 
            // send_packet.data_size = 0;
            sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

            unsigned int packet_count = 0;
            Packet packets[100];
            int start_loop = 0;
            while (1){
                // Receive packets from server
                if (recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, NULL, NULL) < 0){
                    printf("No Acknowledge: %s\nTrying again...",strerror(errno));
                    sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
                    sleep(1);
                    continue;
                } 
                else { //got an ACK
                    if (received_packet.op_id == -1){ // if file open error or other error
                        printf("%s\n", received_packet.data);
                        break; 
                    }
                                     packets[received_packet.op_id -1] = received_packet;
                    packet_count++;
                
                    // last file packet:
                    if (strlen(received_packet.data) < DATA_SIZE-1){//} && (received_packet.packet_num == packet_count)){
                        // TODO: what if the last packet contains data size of 992
                        
                        // Reconstruct the file from all packets
                        fd = fopen(send_packet.file_name, "w");
                        for (int i = 0; i < packet_count; i++) {
                            if (packets[i].op_id != i+1) {
                                printf("Error: Missing packet #%d.\nPlease try again...\n", i+1);
                                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
                                start_loop =1;
                                break;
                            }
                            size_t write_count = fwrite(packets[i].data,1, strlen(packets[i].data), fd);
                            if (write_count != strlen(packets[i].data)) {
                                printf("Error writing to file\n");
                                start_loop = 1;
                                break;
                            }
                        }
                        if(start_loop) break;
                        printf("File Dowloaded successfully\n");
                        fclose(fd);
                        break;                                       
                    }
                }
            }
            break;
        case 'w':
        case 'W':
            send_packet.op_id = 0;    
            strncpy(send_packet.file_name, filename_in(), sizeof(send_packet.file_name) - 1); 

            fd = fopen(send_packet.file_name, "r");
            if (fd == NULL) {
                printf("Error opening file: %s!\n",strerror(errno));
                break;
            }
            int n_read;
            //reading the file and sending packets:
            while((n_read = fread(send_packet.data, 1, DATA_SIZE-1, fd)) > 0){ 
                if (n_read < DATA_SIZE-1 && feof(fd)){
                    memset(send_packet.data+n_read,0,DATA_SIZE-n_read);
                }
                send_packet.data[n_read]='\0';
                send_packet.op_id++;

                // Send the packet to server
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
                // Receive response from server
                if (recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, NULL, NULL) < 0){
                    printf("No Acknowledge: %s\nTrying again...",strerror(errno));
                    // Send the packet again:
                    if (fseek(fd, -n_read, SEEK_CUR) != 0) {
                        printf("Error seeking file: %s", strerror(errno));
                        printf("could not upload the file.\n");
                        exit(1);
                    }
                    send_packet.op_id--;
                    sleep(1);
                    continue;
                }
                else {
                    // print Acknowledge:
                    printf("%s", received_packet.data);
                }
            }
            //end of file:
            if (recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, NULL, NULL) < 0)
                perror("Receive from server failed");
            else {
                printf("%s", received_packet.data);
                // error occured- missing packet:
                if (received_packet.op_id == -1){
                    int lost_pack_num;
                    sscanf(received_packet.data, "Error: Missing packet #%d", &lost_pack_num); 
                    if (fseek(fd, (lost_pack_num-1)*DATA_SIZE, SEEK_SET) != 0) {
                        printf("Error seeking file: %s", strerror(errno));
                        printf("could not upload the file. Please try again\n");
                        continue;
                    }
                    else{
                        //send packet again:
                        n_read = fread(send_packet.data, 1, DATA_SIZE, fd);
                        send_packet.op_id = lost_pack_num;
                        sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
                    }
                }
            }
            fclose(fd);
            break;
        case 'd':
        case 'D':
            send_packet.op_id = 0;
            strncpy(send_packet.file_name, filename_in(), sizeof(send_packet.file_name) - 1); 
            //send request:
            sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
            // Ack:
            while (recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, NULL, NULL) < 0){
                printf("No Acknowledge: %s\nTrying again...",strerror(errno));
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
                sleep(1);
            }
            printf("%s", received_packet.data);
            break;
        
        default:
            printf("Invalid input:\n");
            continue;
        }
    }
    close(sockfd);
    return 0;
}
