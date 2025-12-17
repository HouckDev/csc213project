#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include "message.h"
#include "socket.h"
// Starter networking code recieved from CSC-213 Networking Exercise
int socket_fd;

// Client input thread, get input lines and submit them to the server
void *client_input(void *arg)
{
  while (1)
  {
    // Send a message to the server
    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
    line[strlen(line) - 1] = 0;
    int rc = send_message(socket_fd, line);
    printf(">%s\n", line);
    if (rc == -1)
    {
      perror("Failed to send message to server");
      exit(EXIT_FAILURE);
    }

    if (strcmp(line, "quit") == 0)
    {
      break;
    }
  }
  return NULL;
}

// Server input thread, wait for message, then print all text recieved from server
void *client_output(void *arg)
{

  while (1)
  {
    // Read a message from the server
    char *message = receive_message(socket_fd);
    if (message == NULL)
    {
      perror("Failed to read message from server");
      exit(EXIT_FAILURE);
    }

    // Print the message
    printf("%s\n", message);

    // Free the message
    free(message);
  }
  return NULL;
}
int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <server name> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Read command line arguments
  char *server_name = argv[1];
  unsigned short port = atoi(argv[2]);

  // Connect to the server
  socket_fd = socket_connect(server_name, port);
  if (socket_fd == -1)
  {
    perror("Failed to connect");
    exit(EXIT_FAILURE);
  }
  pthread_t input_thread;
  pthread_t output_thread;
  pthread_create(&input_thread, NULL, client_input, NULL);
  pthread_create(&output_thread, NULL, client_output, NULL);

  printf("';' to to issue commands\n");
  printf("'q'/'quit' to disconnect\n");
  printf("'e' to emote\n");
  printf("'m' to move/enter\n");
  printf("'l' to examine\n");
  // Close socket
  pthread_join(input_thread,NULL);
  pthread_join(output_thread,NULL);
  close(socket_fd);

  return 0;
}
