/*
 * tcp_server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <stdint.h>

#define FILE_BUFFER 256

typedef struct {
    char f; // First magic number 'P'.
    char s; // Second magic number 'O'.
    uint8_t opcode; // Opcode.
    uint8_t nameLen; // Filename length.
    uint8_t bufferLen; // Length of file in this buffer.
    uint32_t fileLen; // Number of bytes in the file.
    char name[256]; // Name of the file.
    unsigned char data[FILE_BUFFER]; // Part or all of the file.
} msg;

void *worker_thread(void *arg) {

    int ret;
    int connfd = (int)(unsigned long)arg;
    char recv_buffer[1024];
    msg mm;
    FILE * sendToClient;
    FILE * receiveFromClient;

    printf("[connfd-%d] worker thread started.\n", connfd);

    while (1) {
        
        /* Clear the message. */
        memset(&mm,0,sizeof(msg));

        /* Get the first packet.  */
        ret = recv(connfd,&mm,sizeof(msg),0);
        
        /* File is being uploaded. */
        if( mm.f == 'P' && mm.s == 'O' && mm.opcode == 0x80 ) {
            
            /* Create or modify the file. */
            receiveFromClient = fopen(mm.name,"wb"); // Create file.

            /* Coutner keeping track of what byte is being written. */
            uint32_t kk;

            /* Iterate over all bytes in file. */
            for( uint32_t jj = 0; jj < mm.fileLen; jj+=(FILE_BUFFER) ) {
                
                /* Write each byte to the file. */
                for( kk = 0; kk <= mm.bufferLen && kk+jj < mm.fileLen; kk++ ) {
                    fwrite(&mm.data[kk],1,1,receiveFromClient);
                    fflush(receiveFromClient);
                }

                /* Clear data buffer. */
                memset(&mm.data,0,sizeof(msg));

                /* Reached the end of the file, don't get next chunk. */
                if( kk+jj < mm.fileLen ) {
                    ret = recv(connfd,&mm,sizeof(msg),0);
                }

            }

            /* Close the uploaded file. */
            fclose(receiveFromClient);
            
            /* Update message to ACK opcode. */
            mm.opcode = 0x81;
            memset(&mm.data,0,sizeof(msg));

            /* Send back ACK. */
            ret = send(connfd,&mm,sizeof(msg),0);
        /* File is being downloaded. */
        } else if( mm.f == 'P' && mm.s == 'O' && mm.opcode == 0x82 ) {

            sendToClient = fopen(mm.name,"rb");

            if( sendToClient ) { // File exists.
                mm.opcode = 0x70;

                mm.nameLen = strlen(mm.name);

                /* Count bytes to the end of the file and save size. */
                fseek(sendToClient,0L,SEEK_END);
                mm.fileLen = (uint32_t) ftell(sendToClient);

                /* Put file pointer back to beginning of the file. */
                fseek(sendToClient,0L,SEEK_SET);

                uint32_t ll;

                /* Iterate over all bytes in file. */
                for( uint32_t jj = 0; jj < mm.fileLen; jj+=(FILE_BUFFER) ) {

                    /* Clear data buffer. */
                    memset(&mm.data,0,sizeof(mm.data));

                    /* Get FILE_BUFFER bytes from the file and send them. */
                    for( ll = 0; ll < FILE_BUFFER && ll+jj < mm.fileLen; ll++ ) {
                        fread(&mm.data[ll],1,1,sendToClient);
                    }

                    /* Update buffer length. */
                    mm.bufferLen = 255;

                    /* Send the chunk.  */
                    ret = send(connfd,&mm,sizeof(msg),0);

                    /* Wait for client to catch up. */
                    usleep(1000); // Stop race condition.
                }

                /* Close file. */
                fclose(sendToClient);

                /* Update to confirmation opcode. */
                mm.opcode = 0x83;

                /* Clear data. */
                memset(&mm.data,0,sizeof(mm.data));

                ret = send(connfd,&mm,sizeof(msg),0);

            } else { // File does not exist.
                mm.opcode = 0x71;
                ret = send(connfd,&mm,sizeof(msg),0);
            }

        }

        if (ret < 0) {
            printf("[connfd-%d] recv() error: %s.\n", connfd, strerror(errno));
            return NULL;
        } else if (ret == 0) {
            printf("[connfd-%d] connection finished\n", connfd);
            break;
        }

        if (ret < 0) {
            printf("send() error: %s.\n", strerror(errno));
            break;
        }

        printf("[connfd-%d] %s", connfd, recv_buffer);
    } // End of infinite while loop.

    printf("[connfd-%d] worker thread terminated.\n", connfd);

    return NULL;
}

int main(int argc, char *argv[]) {
    int ret;
    socklen_t len;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(31000);

    ret = bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        printf("bind() error: %s.\n", strerror(errno));
        return -1;
    }

    if (listen(listenfd, 10) < 0) {
        printf("listen() error: %s.\n", strerror(errno));
        return -1;
    }

    while (1) {
        printf("waiting for connection...\n");
        connfd = accept(listenfd, (struct sockaddr*) &client_addr, &len);

        if (connfd < 0) {
            printf("accept() error: %s.\n", strerror(errno));
            return -1;
        }
        printf("connection accept from %s.\n", inet_ntoa(client_addr.sin_addr));

        pthread_t tid;
        pthread_create(&tid, NULL, worker_thread, (void *)(unsigned long)connfd);

    }
    return 0;
}
