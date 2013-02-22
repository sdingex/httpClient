#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define HOST_ADDRESS "127.0.0.1"
#define PORT 80
#define MAX_RETRIES 5

int sockfd;
struct sockaddr_in address;

int read_line(int fd, char *buffer, int size) {
    char next = '\0';
    char err;
    int i = 0;
    while ((i < (size - 1)) && (next != '\n')) {
        err = read(fd, &next, 1);

        if (err <= 0) break;

        if (next == '\r') {
            err = recv(fd, &next, 1, MSG_PEEK);
            if (err > 0 && next == '\n') {
                read(fd, &next, 1);
            } else {
                next = '\n';
            }
        }
        buffer[i] = next;
        i++;
    }
    buffer[i] = '\0';
    return i;
}

int read_socket(int fd, char *buffer, int size) {
    int bytes_recvd = 0;
    int retries = 0;
    int total_recvd = 0;

    while (retries < MAX_RETRIES && size > 0 && strstr(buffer, ">") == NULL) {
        bytes_recvd = read(fd, buffer, size);

        if (bytes_recvd > 0) {
            buffer += bytes_recvd;
            size -= bytes_recvd;
            total_recvd += bytes_recvd;
        } else {
            retries++;
        }
    }

    if (bytes_recvd >= 0) {
        // Last read was not an error, return how many bytes were recvd
        return total_recvd;
    }
    // Last read was an error, return error code
    return -1;
}

int write_socket(int fd, char *msg, int size) {
    int bytes_sent = 0;
    int retries = 0;
    int total_sent = 0;

    while (retries < MAX_RETRIES && size > 0) {
        bytes_sent = write(fd, msg, size);

        if (bytes_sent > 0) {
            msg += bytes_sent;
            size -= bytes_sent;
            total_sent += bytes_sent;
        } else {
            retries++;
        }
    }

    if (bytes_sent >= 0) {
        // Last write was not an error, return how many bytes were sent
        return total_sent;
    }
    // Last write was an error, return error code
    return -1;
}

void print_usage() {
    printf("Usage: ./httpget http://address/file.extension\n");
}

char* split_path(char *path, char **file_out) {
    char *pos = strrchr(path, '/');
    if (pos == NULL) {
        char *addr = (char*) malloc(strlen(path) + 1);
        char *file = (char*) malloc(2);
        strncpy(addr, path, strlen(path));
        strncpy(file, "/", 1);
        *file_out = file;
        return addr;
    }
    int len_addr = pos - path;
    int len_file = strlen(path) - (pos - path);
    
    char *addr = (char*) malloc(len_addr + 1);
    char *file = (char*) malloc(len_file + 1);

    strncpy(addr, path, len_addr);
    addr[len_addr] = '\0';

    strcpy(file, pos);

    *file_out = file;
    return addr;
}

int split_port(char *addr) {
    char *pos = strrchr(addr, ':');
    if (pos == NULL) {
        return PORT;
    }
    int port = atoi(pos + 1);
    *pos = '\0';
    return port;
}

int main(int argc, char *argv[]) {
    int arg_count = argc - 1;
    if (arg_count < 1 || arg_count > 1) {
        print_usage();
        return 1;
    }
    
    // The host address and file we wish to GET, e.g. http://127.0.0.1/blah.txt
    char *addr_start = argv[1];

    // Skip past http:// if it's there
    if (strncasecmp(addr_start, "http://", strlen("http://")) == 0) {
        addr_start += strlen("http://");
    }

    // Split the host address into a web address and a file name/path, 
    // e.g. 127.0.0.1/blah.txt into 127.0.0.1 and /blah.txt
    char *file_name;
    char *server_address = split_path(addr_start, &file_name);
    int port = split_port(server_address);
    struct hostent *host_entity = gethostbyname(server_address);

    printf("Get file %s at server %s on port %d\n", file_name, server_address, port);

    // Open up a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Unable to open socket\n"); 
        exit(1);
        return 1;
    }

    // Try to connect to server_address on port PORT
    address.sin_family = AF_INET;
    memcpy(&address.sin_addr, host_entity->h_addr_list[0], host_entity->h_length);
    // address.sin_addr.s_addr = inet_addr(server_address);
    address.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*) &address, sizeof(address)) < 0) {
        printf("Unable to connect to host\n");
        exit(1);
        return 1;
    }

    printf("Connected with host\n");

    /*******************************************************/
    /*             Begin sending GET request               */
    /*******************************************************/
    char buffer[8096];
    sprintf(buffer, "GET %s HTTP/1.1\r\n", file_name);
    write_socket(sockfd, buffer, strlen(buffer));
    sprintf(buffer, "Host: %s:%d\r\n", server_address, port);
    write_socket(sockfd, buffer, strlen(buffer));
    sprintf(buffer, "User-Agent: Test HTTP Client\r\n");
    write_socket(sockfd, buffer, strlen(buffer));
    sprintf(buffer, "Connection: close\r\n");
    write_socket(sockfd, buffer, strlen(buffer));

    // Signal end of headers with empty line
    sprintf(buffer, "\r\n");
    write_socket(sockfd, buffer, strlen(buffer));

    /*******************************************************/
    /*             Begin reading response                  */
    /*******************************************************/

    // Read first line and print to console
    int len = read_line(sockfd, buffer, sizeof(buffer));
    fprintf(stderr, "%s\n", buffer);

    // Close the connection down
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
}
