/* CSci4061 F2015 Assignment 4
*  name: Jacob Grafenstein, no partner
*  id: grafe014
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#define MAX_CITY_LENGTH 15
#define MAX_TWITTER_LINE 100
#define BUFFER_SIZE 256

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n, i;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char *fileStream = malloc(11*sizeof(char));
    char *token = malloc(MAX_TWITTER_LINE*sizeof(char));
    char *newFileStream = malloc(MAX_CITY_LENGTH*sizeof(char));
    char line[MAX_CITY_LENGTH];
    char buffer[BUFFER_SIZE];
    char trends[MAX_TWITTER_LINE];
    FILE *myFile;
    FILE *newFile;


    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);

    // Create socket for the client
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      error("ERROR opening socket");
    }
    // Find my server
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    // Set up information about the server
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    // Attempt to connect to the server
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
      error("ERROR connecting");
    }
    bzero(buffer, BUFFER_SIZE);
    printf("client connects\n");

    // Waiting for handshake message from server
    n = read(sockfd, buffer, BUFFER_SIZE-1);
    if (n < 0) {
      error("ERROR on read from server");
    }

    // If incorrect protocol, exit with an error
    if (!strcmp(buffer, "100,0,\n")) {
      perror("Incorrect communication protocol from server; expected: '100,0,'");
      exit(1);
    }
    bzero(buffer,BUFFER_SIZE);

    // Writing handshake response to server
    printf("client sends handshake response:(101,0,)\n");
    n = write(sockfd, "101,0,", 6);
    if (n < 0) {
      error("ERROR on write to server");
    }

    // This is to prevent the client from writing a second time before the server is ready.
    n = read(sockfd, buffer, BUFFER_SIZE-1);
    if (n < 0) {
      error("ERROR on read from server");
    }
    bzero(buffer, BUFFER_SIZE);

    // This for loop is designed to handle all extra clients that have been added to the end of the call in the shell
    for (i = 3; i < argc; i++) {
      // Open both client file and .result file
      strcpy(fileStream, argv[i]);
      myFile = fopen(fileStream, "r");
      strcpy(newFileStream, fileStream);
      strcat(newFileStream, ".result");
      newFile = fopen(newFileStream, "w");

      // Get each line in the file
      while (fgets(line, sizeof(line), myFile) != NULL) {
        // Delete the newline at the end of the line
        size_t ln = strlen(line) - 1;
        if (line[ln] == '\n') {
          line[ln] = '\0';
        }

        // Send first twitterTrend request to the server
        sprintf(buffer, "102,%d,%s",strlen(line), line);
        printf("client sends twitterTrend request:(102,%d,\"%s\")\n", strlen(line), line);
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0) {
          error("ERROR on write to server");
        }
        bzero(buffer, BUFFER_SIZE);

        // Read from the server for synchronization
        n = read(sockfd, buffer, BUFFER_SIZE-1);
        if (n < 0) {
          error("ERROR on read from server");
        }

        // Pick apart the pieces of the server message, copy the last piece into trend buffer
        token = strtok(buffer, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        strcpy(trends,token);
        // If the token is NA, do nothing. Otherwise, split apart the different trends of the response
        if (strcmp(token, "NA")) {
          strcat(trends,",");
          token = strtok(NULL, ",");
          strcat(trends,token);
          strcat(trends, ",");
          token = strtok(NULL, ",");
          strcat(trends, token);
        }

        // Print the city and trends to the file
        n = fprintf(newFile, "%s : %s\n", line, trends);
        fflush(newFile);
        if (n < 0) {
          perror("fprintf failed.");
        }
        bzero(buffer, BUFFER_SIZE);

        // Write to server for synchronization
        n = write(sockfd, "107,0,", 6);
        if (n < 0) {
          error("ERROR on write to server");
        }

        // Read an end of response message from the server
        n = read(sockfd, buffer, BUFFER_SIZE-1);
        if (n < 0) {
          error("ERROR on read from server");
        }
        if (!strcmp(buffer, "105,0")) {
          error("ERROR on end of response from server");
        }
        bzero(buffer, BUFFER_SIZE);
      }
      // Close your files
      fclose(myFile);
      fclose(newFile);
    }

    // Send end of request to the server
    printf("client sends end of request:(104,0,)\n");
    n = write(sockfd, "104,0,", 6);
    if (n < 0) {
      error("ERROR on writing end of request to server");
    }
    // Close your socket
    printf("client closes connection\n");
    close(sockfd);
    return 0;
}
