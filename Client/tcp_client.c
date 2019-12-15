/*
 * tcp_client.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#define UP_PREFIX_LEN 7   // upload$
#define DOWN_PREFIX_LEN 9 // download$
#define FILE_BUFFER 256

typedef struct {
    char f;            // First magic number 'P'.
    char s;            // Second magic number 'O'.
    uint8_t opcode;    // Opcode.
    uint8_t nameLen;   // Filename length.
    uint8_t bufferLen; // Length of file in this buffer.
    uint32_t fileLen;  // Number of bytes in the file.
    char name[256];    // Name of the file.
    unsigned char data[FILE_BUFFER]; // Part or all of the file.
} msg;

/* Expected prefix for uploading a file. */
const char upPrefix[UP_PREFIX_LEN] = {'u','p','l','o','a','d','$'};

/* Expected prefix for downloading a file. */
const char downPrefix[DOWN_PREFIX_LEN] = {'d','o','w','n','l','o','a','d','$'};

int main(int argc, char *argv[]) {

    /* Return type from the socket connection attempt. SHould return 0 on success
     * and some ERRNO on failure.
     */
    int ret;

    /* File destriptor for the socket based on teh output of the socket(). */
    int sockfd = 0;

    /* Buffer for reading text from the keyboard.  */
    char send_buffer[1024];

    /* Struct containing important information and properties about the socket.  */
    struct sockaddr_in serv_addr;

    /* Variable for checking upload$_ prefix error. */
    int upError = 0;

    /* Varialbe for checking the download$_ prefix error.  */
    int downError = 0;

    /* FILE that you want to upload. */
    FILE * toUpload;

    /* FILE that you want to download from server. */
    FILE * toDownload;

    int ii;

    struct timespec tim, tim2;

    tim.tv_sec = 0;
    tim.tv_nsec = 1000000L;

    /* Message that will upload file. */
    msg m;

    /* Get file descriptor for the socket type and handle errors.  */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket() error: %s.\n", strerror(errno));
        return -1;
    }

    /* Clear the sockaddr_in struct for server info,  */
    memset(&serv_addr, 0, sizeof(serv_addr));

    /* Define the socket as IPv4 Internet protocol.  */
    serv_addr.sin_family = AF_INET;

    /* Set the address and port number of the destination.  */
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(31000);

    /* Attempt to connect to the server and handle error if can't connect.  */
    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        printf("connect() error: %s.\n", strerror(errno));
        return -1;
    }

    /* Read from keyboard and send/receive information from the server. */
    while (1) {

        /* Clear the file name. */
        memset(&m.name,0,sizeof(m.name));

        /* Clear the message to be sent. */
        memset(&m,0,sizeof(msg));
        m.f = 'P';
        m.s = 'O';

        /* Clear the send buffer and prepare for keyboard input. */
        memset(&send_buffer,0,sizeof(send_buffer));

        /* Read from keyboard input. */
        fgets(send_buffer, sizeof(send_buffer),stdin);

        /* Check for 'exit' command. */
        if ( strncmp(send_buffer, "exit", strlen("exit")) == 0 ) {
            shutdown(sockfd, SHUT_RDWR);
            break;
        /* Upload file. */
        } else if( strncmp(send_buffer, upPrefix, sizeof(upPrefix)) == 0 ) {

            if( send_buffer[UP_PREFIX_LEN] == '\n' ) { // No file was specified.
                printf("No file was specified.\n");
            } else { // A file was specified.

                memset(&m.name,0,sizeof(m.name));

                ii = 0;

                /* Extract the file name. */
                while( send_buffer[ii+UP_PREFIX_LEN] != '\n' ) {
                    m.name[ii] = send_buffer[ii+UP_PREFIX_LEN];
                    ii++;
                }
                
                /* Try to open the file specified. */
                toUpload = fopen(m.name,"rb");

                if( toUpload ) { // File was found.

                    /* Update length of the name. */
                    m.nameLen = ii;

                    /* Update opcode. */
                    m.opcode = 0x80; // Upload opcode.

                    /* Count bytes to the end of the file and save size. */
                    fseek(toUpload,0L,SEEK_END);
                    m.fileLen = (uint32_t) ftell(toUpload);

                    /* Put file pointer back to beginning of the file. */
                    fseek(toUpload,0L,SEEK_SET);
                    
                    uint32_t kk;

                    /* Iterate over all bytes in file. */
                    for( uint32_t jj = 0; jj < m.fileLen; jj+=(FILE_BUFFER) ) {

                        /* Clear data buffer. */
                        memset(&m.data,0,sizeof(m.data));
                        
                        /* Get FILE_BUFFER bytes from the file and send them. */
                        for( kk = 0; kk < FILE_BUFFER && kk+jj < m.fileLen; kk++ ) {
                            fread(&m.data[kk],1,1,toUpload);
                        }

                        /* Update buffer length. */
                        m.bufferLen = 255;

                        /* Send the chunk.  */
                        ret = send(sockfd,&m,sizeof(msg),0);

                        /* Wait for server to catch up. */
                        usleep(1000); // Stop race condition.
                    }
                    
                    /* Close file. */
                    fclose(toUpload);

                    /* Get ACK. */
                    ret = recv(sockfd,&m,sizeof(msg),0);

                    /* Check ACK response from server. */
                    if( m.f == 'P' && m.s == 'O' && m.opcode == 0x81 ) {
                        printf("upload_ack$file_upload_successfully!\n");
                    }

                } else { // File was not found, print error.
                    printf("Could not find the file in this directory...\n");
                }
            }

        /* Download file. */
        } else if( strncmp(send_buffer, downPrefix,sizeof(downPrefix)) == 0 ) {
            if( send_buffer[DOWN_PREFIX_LEN] == '\n' ) { // No file was specified.
                printf("No file was specified...\n");
            } else { // A file was specified.
                
                /* Download opcode. */
                m.opcode = 0x82;

                memset(&m.name,0,sizeof(m.name));

                ii = 0;

                /* Extract the file name. */
                while( send_buffer[ii+DOWN_PREFIX_LEN] != '\n' ) {
                    m.name[ii] = send_buffer[ii+DOWN_PREFIX_LEN];
                    ii++;
                }

                m.nameLen = ii;

                /* Empty buffer length since we are not sending a file. */
                m.bufferLen = 0;

                /* Empty file length since we are not sending a file. */
                m.fileLen = 0;

                /* Clear the payload since we are not sending a file. */
                memset(&m.data,0,sizeof(m.data));

                /* Send download request. */
                ret = send(sockfd,&m,sizeof(msg),0);

                /* Get response back from server. */
                ret = recv(sockfd,&m,sizeof(msg),0);

                /* Check if file exists on the server and can be downloaded. */
                if( m.f == 'P' && m.s == 'O' && m.opcode == 0x70 ) { // File exists on server.

                    printf("File exists! Downloading...\n");

                    /* Create or modify the file. */
                    toDownload = fopen(m.name,"wb"); // Create file.

                    /* Coutner keeping track of what byte is being written. */
                    uint32_t ll;

                    /* Iterate over all bytes in file. */
                    for( uint32_t jj = 0; jj < m.fileLen; jj+=(FILE_BUFFER) ) {

                        /* Write each byte to the file. */
                        for( ll = 0; ll <= m.bufferLen && ll+jj < m.fileLen; ll++ ) {
                            fwrite(&m.data[ll],1,1,toDownload);
                            fflush(toDownload);
                        }

                        /* Clear data buffer. */
                        memset(&m.data,0,sizeof(msg));

                        /* Reached the end of the file, don't get next chunk. */
                        if( ll+jj < m.fileLen ) {
                            ret = recv(sockfd,&m,sizeof(msg),0);
                        }

                    }

                    /* Close the downloaded file. */
                    fclose(toDownload);

                    ret = recv(sockfd,&m,sizeof(msg),0);

                    if( m.f == 'P' && m.s == 'O' && m.opcode == 0x83 ) {
                        printf("download_ack$file_download_successfully!\n");
                    }

                } else if( m.f == 'P' && m.s == 'O' && m.opcode == 0x71 ) { // Does not exist.
                    printf("File does not exist on server...\n");
                }
            }
        /* Print an error message. */
        } else {
            printf("Command not recognized...\n");
        }
    }

    close(sockfd);

    return 0;
}
