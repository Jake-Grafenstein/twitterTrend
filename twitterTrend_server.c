/* CSci4061 F2015 Assignment 4
*  name: Jacob Grafenstein, no partner
*  id: grafe014
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define MAX_CITY_LENGTH 15
#define MAX_CITIES 50
#define MAX_INPUTS 100
#define MAX_TWITTER_LINE 100
#define BUFFER_SIZE 256

sem_t empty, mutex, full;

struct twitterEntry {
  char name[MAX_CITY_LENGTH];
  char trends[MAX_TWITTER_LINE];
};

struct twitterEntry *twitterDB;
int *socketQueue;
char **IPQueue;
int num_entries;
int clientCounter = 0;
int myIndex = 0;
int flag = 0;

// Creates the database using the previously defined structure for cities and trending items. First, it creates the structure on the heap. Then, the file is opened and looped over line by line. Using strtok, the line is broken into pieces by "," and stored in the correct spots for the element in the structure. After all lines have been looped over, the function returns the twitterDB.
void createDB(const char *dbname) {
  int newCounter;
  char line[MAX_TWITTER_LINE];
  const char *s = ",";
  char *token;

  // Open file and copy each line
  FILE *DBfile = fopen(dbname, "r");
  newCounter = 0;
  while(fgets(line, sizeof(line), DBfile) != NULL) {
    // Get the city name from the line
    token = strtok(line, s);
    strcpy(twitterDB[newCounter].name,token);

    // Get the trends from the line
    token = strtok(NULL, s);
    strcpy(twitterDB[newCounter].trends, token);
    while((token = strtok(NULL, s)) != NULL) {
      strcat(twitterDB[newCounter].trends, ",");
      strcat(twitterDB[newCounter].trends,token);
    }
    strcat(twitterDB[newCounter].trends, "\0");
    newCounter++;
  }
  num_entries = newCounter;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

// This function creates instances of all threads and serves the trends to the client
void *handler(void *arg) {
  int n, i, ln;
  char buffer[BUFFER_SIZE];
  char messageID[3];
  char messageSize[5];
  char message[MAX_TWITTER_LINE];
  char clientIP[20];
  int threadID = arg;
  int socketid, lineSize;
  char *token = malloc(MAX_TWITTER_LINE*sizeof(char));
  char *myTrends = malloc(MAX_TWITTER_LINE*sizeof(char));
  char *myCity = malloc(MAX_CITY_LENGTH*sizeof(char));
  // Zeroes out the buffer
  bzero(buffer,BUFFER_SIZE);

  while (1) {
    sem_wait(&full);
    sem_wait(&mutex);
      socketid = socketQueue[myIndex];
      strcpy(clientIP, IPQueue[myIndex]);
      myIndex++;
    sem_post(&mutex);
    sem_post(&empty);
    printf("Thread %d is handling client %s\n", threadID, clientIP);

    // Server sends handshaking: (100,0,)
    n = write(socketid, "100,0,", 6);
    printf("server sends handshaking:(100,0,)\n");
    if (n < 0) {
      error("ERROR writing to socket");
    }

    // Server receives handshake response (101,0,)
    n = read(socketid, buffer, BUFFER_SIZE-1);
    if (n < 0) {
      error("ERROR reading from socket");
    }
    if (!strcmp(buffer, "101,0,\n")) {
      perror("Incorrect communication protocol; did not receive 101,0,");
    }
    bzero(buffer,BUFFER_SIZE);

    // Server sends end of request (for synchronization with client)
    n = write(socketid, "105,0,", 6);
    if (n < 0) {
      error("ERROR writing to client");
    }

    // Server receives first real message from Client
    n = read(socketid, buffer, BUFFER_SIZE-1);
    if (n < 0) {
      error("ERROR reading from socket");
    }

    // Server waits until it receives a message other than a TwitterTrend request
    while (!strncmp(buffer, "102,", 4)) {
      // Get the pieces of the message: ID, payload length, and payload
      token = strtok(buffer, ",");
      strcpy(messageID, token);
      token = strtok(NULL,",");
      strcpy(messageSize, token);
      token = strtok(NULL, ",");
      strcpy(message, token);

      // Access the database
      for (i = 0; i < num_entries; i++) {
        // Check if the line is equal to the city name, add name and trends to buffers
        if (!strcmp(twitterDB[i].name, message)) {
          strcpy(myCity, twitterDB[i].name);
          strcpy(myTrends, twitterDB[i].trends);
          flag = 1;
          ln = strlen(myTrends) - 1;
          if (myTrends[ln] == '\n') {
            myTrends[ln] = '\0';
          }
          break;
        }
      }

      // Write the answer to the client. If the flag hasn't been set, then the city wasn't in the database, so send back "NA"
      bzero(buffer, BUFFER_SIZE);
      if (flag == 0) {
        strcpy(myTrends, "NA");
        lineSize = strlen(myTrends);
        sprintf(buffer, "103,%d,%s",lineSize,myTrends);
      } else {
        lineSize = strlen(myTrends);
        sprintf(buffer, "103,%d,%s",lineSize,myTrends);
      }
      printf("server sends twitterTrend response:(103,%d,\"%s\")\n", lineSize,myTrends);
      n = write(socketid, buffer, strlen(buffer));
      if (n < 0) {
        error("ERROR writing to socket");
      }
      bzero(buffer, BUFFER_SIZE);

      // Read from the client for synchronization
      n = read(socketid, buffer, BUFFER_SIZE-1);
      if (n < 0) {
        error("ERROR writing to socket");
      }

      // Write end of response to client
      printf("server sends end of response:(105,0,)\n");
      n = write(socketid, "105,0,", 6);
      if (n < 0) {
        error("ERROR writing end of response to client");
      }

      // Read next request from client.
      n = read(socketid, buffer, BUFFER_SIZE-1);
      if (n < 0) {
        error("ERROR reading from socket");
      } else if (n < 6) {
        read(socketid,buffer, BUFFER_SIZE-1);
      }
      // Reset flag to Zero
      flag = 0;
    }
    // Close my connection
    printf("server closes the connection\n");
    printf("Thread %d finished handling client %s\n", threadID, clientIP);
    close(socketid);
  }
}

// The main function allocates space, creates my socket connnections, creates my threads, and listens for new connections and adds them to the queue.
int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     int n, num_threads;
     pthread_t *threads;
     int i;
     unsigned int clientPort;
     char port[5];
     char line[20];

     // Allocate space for database and queue
     twitterDB = malloc(MAX_CITIES*sizeof(struct twitterEntry));
     socketQueue = (int *) malloc(MAX_INPUTS*sizeof(int *));
     IPQueue = (char **) malloc(MAX_INPUTS*sizeof(char *));
     for (i = 0; i < MAX_INPUTS; i++) {
       IPQueue[i] = malloc(20*sizeof(char));
     }

     // Handle if the thread parameter wasn't included.
     if (argc < 3) {
       num_threads = 5;
     } else if (argc == 3) {
       num_threads = atoi(argv[2]);
     }

     // Call createDB(dbname) to parse the database file and add cities and trends to the database in structs
     char const *dbName = "TwitterDB.txt";
     createDB(dbName);

     // Initialize my semaphores
     sem_init(&mutex, 0, 1);
   	 sem_init(&empty, 0, num_threads);
   	 sem_init(&full, 0, 0);

     // Allocate space for my threads and create them
     threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
     for (i = 0; i < num_threads; i++)
     {
       pthread_create(&threads[i], NULL, handler, i);
     }

     // SOCK_STREAM denotes a safe stream of bytes on the sockets, not datagrams
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) {
       error("ERROR opening socket");
     }
     // Zero out my server address (get rid of junk)
     bzero((char *) &serv_addr, sizeof(serv_addr));

     // Grab the first second argument and assign it as my port number
     portno = atoi(argv[1]);
     // Set my server information
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);

     // Bind my server to a socket
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
       error("ERROR on binding");
     }

     n = 0;
     // Listens for num_threads connections
     printf("server listens\n");
     listen(sockfd,num_threads);
     clilen = sizeof(cli_addr);
     while(1) {
       // Creates a new socket for the accepted connection
       newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
       printf("server accepts connection\n");
       clientPort = (unsigned int) ntohs(cli_addr.sin_port);
       sprintf(port, "%u", clientPort);
       if (newsockfd < 0) {
         error("ERROR on accept");
       } else {
         strcpy(line, inet_ntoa(cli_addr.sin_addr));
         strcat(line, ",");
         strcat(line, port);

         // Add socketid to socketQueue, add client information to IPQueue
         sem_wait(&empty);
         sem_wait(&mutex);
           socketQueue[n] = newsockfd;
           strcpy(IPQueue[n], line);
           clientCounter++;
           n++;
         sem_post(&mutex);
         sem_post(&full);

       }
     }
     close(sockfd);
     return 0;
}
