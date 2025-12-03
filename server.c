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

// Actor system
typedef struct Actor actor_t;
struct Actor
{
  char *name;
  actor_t *parentActor;
  actor_t *subActors;
  actor_t *nextSubActor;
  int* address;
};

// Destroy an actor (and its sub actors)
void actor_t_destroy(actor_t actor)
{
  free(actor.name);

  // Free all sub actors of this
  actor_t *node = actor.subActors;
  while (node)
  {
    actor_t *next = node->nextSubActor;
    actor_t_destroy(*node);
    free(node);
    node = next;
  }
}

// Allocate space and populate the fields of a new actor
actor_t *actor_t_create(char *name)
{
  actor_t *actor = malloc(sizeof(actor_t));
  actor->name = name;
  return actor;
}

// Add an actor to the children of an existing actor
void actor_t_attach(actor_t *parent, actor_t *child)
{
  if (parent->subActors)
  {
    actor_t *node = parent->subActors;
    while (node->nextSubActor)
    {
      actor_t *node = node->nextSubActor;
    }
    node->nextSubActor = child;
    child->parentActor = parent;
  }
  else
  {
    parent->subActors = child;
    child->parentActor = parent;
  }
}

actor_t *gameworld;

void *reciever_client(void *arg)
{
  actor_t* client_actor = (actor_t *)arg;
  while (1)
  {
    // Read a message from the client
    char *message = receive_message(*client_actor->address);
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
      { // If this user exists
        if (addresses[i] == *client_actor->address)
        { // If this user is the one who submitted the message (Format in 1st person)
          char formattedString[100];
          sprintf(formattedString, "You say '%s'", message);
          int rc = send_message(addresses[i], formattedString);
          if (rc == -1)
          {
            perror("Failed to send message to client");
            exit(EXIT_FAILURE);
          }
        }
        else
        { // If this user is anyone else (Format in 3rd person)
          char formattedString[100];
          sprintf(formattedString, "%s says '%s'", client_actor->name, message);
          int rc = send_message(addresses[i], formattedString);
          if (rc == -1)
          {
            perror("Failed to send message to client");
            exit(EXIT_FAILURE);
          }
        }
      }
    }
    // Print the message
    printf("%d - %s\n", *client_actor->address, message);
    // Free the message string
    free(message);
  }
  close(*client_actor->address);
  return NULL;
}

int main()
{
  // Initialize the gameworld
  gameworld = actor_t_create("World");

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
  for (int i = 0; i < MAX_USERS; i++)
  {
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
    actor_t *player = actor_t_create("Player");
    actor_t_attach(gameworld, player);
    player->address = &addresses[z];
    pthread_create(&ts[z], NULL, reciever_client, player);
    z++;
  }
  // Close sockets
  close(server_socket_fd);

  // Destroy gameworld
  actor_t_destroy(*gameworld);
  free(gameworld);
  return 0;
}
