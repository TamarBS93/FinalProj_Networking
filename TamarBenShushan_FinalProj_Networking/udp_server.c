#include "udp_file_transfer.h"

int main()
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", SERVER_PORT);
    Packet send_packet;
    Packet received_packet;
    FILE * fd;

    unsigned int packet_count = 0;
    unsigned int start_loop = 0;

    while (1){
        
        // Receive message from client
        int n = recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0){
            perror("Receive failed\n");
            continue;
        }

        if (received_packet.op_id == 0){ // client want to delete a file
            // deleting the file
            if (remove(received_packet.file_name) == 0 ){
                snprintf(send_packet.data, DATA_SIZE,"\"%s\" deleted successfully.\n", received_packet.file_name);
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);
            } 
            else {
                snprintf(send_packet.data, DATA_SIZE,"Error deleting file \"%s\": %s\n", received_packet.file_name, strerror(errno));
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);
            }
        }
        else if (received_packet.op_id == -1){ // client want to download a file
            
            fd = fopen(received_packet.file_name, "r");
            if (fd == NULL) {
                send_packet.op_id = -1;
                snprintf(send_packet.data, DATA_SIZE,"Error downloading file \"%s\": %s\n", received_packet.file_name, strerror(errno));
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);
                continue;
            }

            int n_read;
            send_packet.op_id = 0;
            while((n_read = fread(send_packet.data, 1, DATA_SIZE-1, fd)) > 0){ 
                send_packet.data[n_read]='\0';
                send_packet.op_id++;
                // Send the packet to client
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, sizeof(client_addr));
            }
        }
        else if (received_packet.op_id > 0){ // client want to upload a file
            
            // packets storage and count:
            Packet packets[100];
            packets[received_packet.op_id -1] = received_packet;
            packet_count++;
                    
            // Acknowledge:
            char response[100];
            send_packet.op_id = received_packet.op_id;
            snprintf(send_packet.data, DATA_SIZE,"Packet #%d arrived successfully to server\n", send_packet.op_id);
            // Respond to client:
            sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);
            
            
            // last package:
            if (strlen(received_packet.data) < DATA_SIZE-1){//} && (received_packet.op_id == packet_count)){
                // TODO: what if the last packet contains data size of 993
                
                // Reconstruct the file from all packets
                fd = fopen(received_packet.file_name, "w");
                printf("Uploading file...\n");
                for (int i = 0; i < packet_count; i++) {
                    // missing packet:
                    if (packets[i].op_id != i+1) {
                        snprintf(send_packet.data, DATA_SIZE,"Error: Missing packet #%d\n", i+1);
                        send_packet.op_id = -1;
                        // sending the issue to the client:
                        sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);                  
                        int n = recvfrom(sockfd, &received_packet, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
                        if (n < 0){
                            perror("Receive failed\n");
                            start_loop =1;
                            break;
                        }
                        packets[received_packet.op_id-1]= received_packet;
                        i--;
                        continue;
                    }
                    size_t write_count = fwrite(packets[i].data,1, strlen(packets[i].data), fd);
                }
                if (start_loop) {
                    start_loop=0;
                    continue;
                }
                fclose(fd);
                printf("File upload completed successfully.\n");
                snprintf(send_packet.data, DATA_SIZE,"File upload completed successfully.\n");
                // Respond to client:
                sendto(sockfd, &send_packet, BUFFER_SIZE, 0, (const struct sockaddr *)&client_addr, addr_len);
                packet_count = 0;                  
            }
        }       
    }

    close(sockfd);
    return 0;
}
