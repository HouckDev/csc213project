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
  char *class;
  char *name;
  char *description;
  int state;
  void *data;
  actor_t *parentActor;
  actor_t *subActors;
  actor_t *nextSubActor;
};

// Class Data
typedef struct actorData_door actorData_door_t;
struct actorData_door {
  actor_t *portal;
};
actorData_door_t *actorData_door_create(actor_t *portal) {
  actorData_door_t *data = malloc(sizeof(actorData_door_t));
  data->portal = portal;
  return data;
}

typedef struct actorData_player actorData_player_t;
struct actorData_player {
  int *address;
};
actorData_player_t *actorData_player_create(int *address) {
  actorData_player_t *data = malloc(sizeof(actorData_player_t));
  data->address = address;
  return data;
}

void sleep_ms(int ms) {
  usleep(ms * 1000);
}
// Add an actor to the children of an existing actor
void actor_t_attach(actor_t *parent, actor_t *child)
{
  printf("Attaching actor '%s' to '%s'\n", child->name, parent->name);
  if (parent->subActors)
  {
    actor_t *node = parent->subActors;
    if (node->state == 1)
    {
      parent->subActors = child;
      child->parentActor = parent;
    }
    else
    {
      while (node->nextSubActor && node->nextSubActor->state != 1)
      {
        node = node->nextSubActor;
      }
      node->nextSubActor = child;
      child->parentActor = parent;
    }
  }
  else
  {
    parent->subActors = child;
    child->parentActor = parent;
  }
}

// Destroy an actor (and its sub actors)
void actor_t_destroy(actor_t *actor)
{
  printf("Destroying actor '%s'\n", actor->name);
  actor->state = 1; // Update this actor's state to destroyed
  // Remove actor from parent
  if (actor->parentActor && actor->nextSubActor)
  {
    actor_t_attach(actor->parentActor, actor->nextSubActor);
  }

  // Free all sub actors of this actor
  actor_t *node = actor->subActors;
  while (node)
  {
    actor_t *next = node->nextSubActor;
    actor_t_destroy(node);
    free(node);
    node = next;
  }
  free(actor->data);
}

// Detach an actor (But dont destroy it)
void actor_t_detach(actor_t *actor)
{
  printf("Detaching actor '%s'\n", actor->name);
  actor->state = 1; // Update this actor's state to destroyed
  // Remove actor from parent
  if (actor->parentActor && actor->nextSubActor)
  {
    actor_t_attach(actor->parentActor, actor->nextSubActor);
  }

  if (actor->parentActor->subActors == actor)
  {
    actor->parentActor->subActors = NULL;
  }
  actor_t *node = actor->parentActor->subActors;
  while (node->nextSubActor && node->nextSubActor != actor)
  {
    node = node->nextSubActor;
  }
  node->nextSubActor = NULL;

  actor->nextSubActor = NULL;
  actor->state = 0; // Update this actor's state to normal
  actor->parentActor = NULL;
}

// Allocate space and populate the fields of a new actor
actor_t *actor_t_create(char *class,char *name)
{
  actor_t *actor = malloc(sizeof(actor_t));
  actor->class = name;
  actor->state = 0;
  actor->name = name;
  actor->data = NULL;
  return actor;
}

actor_t *gameworld;

// Broadcast message to the player of the speaker (ownerActor)
void broadcast_private(char *message, actor_t *ownerActor)
{
  int rc = send_message(*((actorData_player_t*)ownerActor->data)->address, message);
  if (rc == -1)
  {
    perror("Failed to send message to client");
    exit(EXIT_FAILURE);
  }
  return;
}

// Broadcast message to all players in same room as the speaker (ownerActor)
void broadcast_local(char *message, actor_t *ownerActor)
{
  printf("Broadcasting message '%s' to area %s\n", message, ownerActor->parentActor->name);
  actor_t *currentActor = ownerActor->parentActor->subActors;
  while (currentActor)
  {
    if (strcmp(currentActor->class,"Player") == 0 && *((actorData_player_t*)currentActor->data)->address)
    { // If this user exists
      if (ownerActor != currentActor)
      { // If this user is anyone else
        int rc = send_message(*((actorData_player_t*)currentActor->data)->address, message);
        if (rc == -1)
        {
          perror("Failed to send message to client");
          exit(EXIT_FAILURE);
        }
      }
    }
    currentActor = currentActor->nextSubActor;
  }
  return;
}

// Broadcast a message to all players regardless of location
void broadcast_global(char *message)
{
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (addresses[i] != -1)
    { // If this user exists
      int rc = send_message(addresses[i], message);
      if (rc == -1)
      {
        perror("Failed to send message to client");
        exit(EXIT_FAILURE);
      }
    }
  }
}

// Get actor by name 
actor_t* actor_find(char *name, actor_t* parent) {
  printf("Searching for actor '%s'\n", name);
  actor_t *currentActor = parent->subActors;
  while (currentActor)
  {
    // Check this actor
    printf("Checking '%s'\n", currentActor->name);
    if (strcmp(name, currentActor->name) == 0)
    {
      printf("Found.\n");
      return currentActor;
    }

    currentActor = currentActor->nextSubActor;
  }
  return NULL;

}

// Execute a game command via string
void executeCommand(actor_t *owner_actor, char* message) {
  if ((strncmp(message, ";l ", 3) == 0) || (strcmp(message, ";l") == 0))
    { // Command move

      char *target = malloc(sizeof(char) * 100);
      sprintf(target, "%s", message);
      target += 3;
      actor_t *currentActor;
    if (strcmp(target, "") == 0) {
      currentActor = owner_actor->parentActor;
    } else {
      // Search for the actor
      printf("Searching for object '%s'\n", target);
      currentActor = actor_find(target,owner_actor->parentActor);
    }
      if (currentActor)
      {
        char formattedString[100];
        sprintf(formattedString, "%s examines %s", owner_actor->name, currentActor->name);
        broadcast_local(formattedString, owner_actor);

        // Print out main objects description
        broadcast_private(currentActor->description, owner_actor);
        // Print out sub objects

        sleep_ms(500);
        actor_t *subActor = currentActor->subActors;
        while (subActor)
        {
          if (subActor != owner_actor) {
        sprintf(formattedString, "You see a '%s'", subActor->name);
              broadcast_private(formattedString, owner_actor);
          }
          subActor = subActor->nextSubActor;
        sleep_ms(500);
        }
      }
      else
      {
        broadcast_private("Could not find object.", owner_actor);
      }
    } else if (strncmp(message, ";m ", 3) == 0)
    { // Command move

      char *target = malloc(sizeof(char) * 100);
      sprintf(target, "%s", message);
      target += 3;

      // Search for the actor
      printf("Searching for door '%s'\n", target);
      actor_t *currentActor = actor_find(target,owner_actor->parentActor);
      if (currentActor && strcmp(currentActor->class,"Door") == 0)
      {
        char formattedString[100];
        sprintf(formattedString, "%s leaves through %s", owner_actor->name, currentActor->name);
        broadcast_local(formattedString, owner_actor);

        actor_t_detach(owner_actor);
        actor_t_attach(((actorData_door_t*)currentActor->data)->portal, owner_actor);
        broadcast_private("You enter the door.", owner_actor);
        sleep_ms(500);
        executeCommand(owner_actor,";l"); // Execute the look command for convienence.

        sprintf(formattedString, "%s enters through %s", owner_actor->name, currentActor->name);
        broadcast_local(formattedString, owner_actor);
      }
      else
      {
        broadcast_private("Could not find door.", owner_actor);
      }
    }
    else if (strncmp(message, ";e ", 3) == 0)
    { // Command emote

      char *formattedStringB = malloc(sizeof(char) * 100);
      sprintf(formattedStringB, "%s", message);
      formattedStringB += 3;
      char formattedString[100];
      sprintf(formattedString, "*%s*", formattedStringB);
      broadcast_private(formattedString, owner_actor);
      sprintf(formattedString, "%s *%s*", owner_actor->name, formattedStringB);
      broadcast_local(formattedString, owner_actor);
      formattedStringB -= 3;
      free(formattedStringB);
    }
    else
    { // Any other case (Speak)

      char formattedString[100];
      sprintf(formattedString, "'%s'", message);
      broadcast_private(formattedString, owner_actor);
      sprintf(formattedString, "%s says '%s'", owner_actor->name, message);
      broadcast_local(formattedString, owner_actor);
    }
}
void *reciever_client(void *arg)
{
  actor_t *client_actor = (actor_t *)arg;
  while (1)
  {
    // Read a message from the client
    char *message = receive_message(*((actorData_player_t*)client_actor->data)->address);
    if (message == NULL)
    {
      perror("Failed to read message from client");
      exit(EXIT_FAILURE);
    }
    if (strcmp(message, ";quit") == 0 || strcmp(message, ";q") == 0)
    { // Command quit
      free(message);
      break;
    }
    executeCommand(client_actor,message);

    
    // Print the message
    printf("%d - %s\n", *((actorData_player_t*)client_actor->data)->address, message);
    // Free the message string
    free(message);
  }
  // Player has disconnected, close their connection and remove their character, inform other players
  // Send a message to all of the clients
  char formattedString[100];
  sprintf(formattedString, "%s has left.", client_actor->name);
  broadcast_global(formattedString);

  close(*((actorData_player_t*)client_actor->data)->address);

  for (int i = 0; i < MAX_USERS; i++)
  {
    if (addresses[i] == *((actorData_player_t*)client_actor->data)->address)
    {
      addresses[i] = -1;
    }
  }
  actor_t_destroy(client_actor);
  free(client_actor);
  return NULL;
}

int main()
{
  // Initialize the gameworld
  gameworld = actor_t_create("World","World");

  // Room 1
  actor_t *room1 = actor_t_create("Room","Room 1");
  room1->description = "You stand in a large room.";
  actor_t_attach(gameworld, room1);

  // Room 2
  actor_t *room2 = actor_t_create("Room","Room 2");
  room2->description = "You stand in a small room.";
  actor_t_attach(gameworld, room2);

  // Populate the gameworld
  // Door 1
  actor_t *door1 = actor_t_create("Door","Door");
  door1->data = actorData_door_create(room2);
  door1->description = "A door leading to a small room.";
  actor_t_attach(room1, door1);
  
  // Key
  actor_t *key1 = actor_t_create("Item","Key");
  key1->description = "An ornate key.";
  actor_t_attach(room1, key1);

  // Door 2
  actor_t *door2 = actor_t_create("Door","Door");
  door2->data = actorData_door_create(room1);
  door2->description = "A door leading to a large room.";
  actor_t_attach(room2, door2);

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
    actor_t *player = actor_t_create("Player","Player");
    player->data = actorData_player_create(&addresses[z]);
    actor_t_attach(room1, player);
    pthread_create(&ts[z], NULL, reciever_client, player);
    z++;
  }
  // Close sockets
  close(server_socket_fd);

  // Destroy gameworld
  actor_t_destroy(gameworld);
  free(gameworld);
  return 0;
}
