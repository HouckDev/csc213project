#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "message.h"
#include "socket.h"

#define MAX_USERS 3
int addresses[MAX_USERS];
// Starter networking code recieved from CSC-213 Networking Exercise
void *reciever_client(void *arg)
{
  int client_socket_fd = *(int *)arg;
  while (1)
  {
    // Read a message from the client
    char *message = receive_message(client_socket_fd);
    if (message == NULL)
    {
      perror("Failed to read message from client");
      exit(EXIT_FAILURE);
    }

    if (strcmp(message, ";quit") == 0 || strcmp(message, ";q") == 0)
    {
      free(message);
      break;
    }

    // Send a message to all of the clients
    for (int i = 0; i < MAX_USERS; i++)
    {
      if (addresses[i] != -1)
      {
        char formattedString[25];
        sprintf(formattedString, "%d - %s", client_socket_fd,message);
        int rc = send_message(addresses[i], formattedString);
        if (rc == -1)
        {
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);
        }
      }
    }
    // Print the message
    printf("%d - %s\n", client_socket_fd, message);
    // Free the message string
    free(message);
  }
  close(client_socket_fd);
  return NULL;
}

int main()
{
  // Open a server socket
  unsigned short port = 0;
  int server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1)
  {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // Start listening for connections, with a maximum of one queued connection
  if (listen(server_socket_fd, 1))
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %u\n", port);
  pthread_t ts[MAX_USERS];
  int z = 0;
  for (int i = 0; i < MAX_USERS; i++){
    addresses[i] = -1;
  }
    // Wait for a client to connect
    while (1)
    {
      addresses[z] = server_socket_accept(server_socket_fd);
      if (addresses[z] == -1)
      {
        perror("accept failed");
        exit(EXIT_FAILURE);
      }

      printf("Client connected!\n");
      // Create the thread
      pthread_create(&ts[z], NULL, reciever_client, &addresses[z]);
      z++;
    }
    // Close sockets
    close(server_socket_fd);

    return 0;
  }
