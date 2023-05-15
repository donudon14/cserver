/*
 * Partially implemented API for the project.
 */
#define _GNU_SOURCE
#include "cserver.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct ServerRec
{
  Boolean shutdown_requested;

  Mutex mutex;
};

typedef struct ClientRec ClientStruct;

struct ClientRec
{
  int conn_fd;

  /* Linked list.  If no next element, this will be NULL.*/
  Client next_client;

  /* Allow overriding IO calls for easier mocking. */
  int (*read)(Client client, char *buf, size_t bytes, void *context);
  void *read_context;

  Boolean (*write)(Client client, char *buf, size_t bytes, void *context);
  void *write_context;
};

Server server_create(void)
{
  Server server = xcalloc(1, sizeof(*server));
  server->mutex = mutex_create();
  return server;
}

void server_destroy(Server server)
{
  if (!server)
    return;

  mutex_destroy(server->mutex);
  xfree(server);
}

void server_shutdown(Server server)
{
  server->shutdown_requested = TRUE;
}

Boolean server_shutdown_requested(Server server)
{
  return server->shutdown_requested;
}

/* Create a Client object, append it to the list of running jobs, start
   a thread and call communicate on the connection.  */
Boolean server_accept_connection(Server server, int conn_fd, char **errors_ret)
{
  /* STUB IMPLEMENTATION, DOES NOT SATISFY REQUIREMENTS. */
  Client client;
  Boolean success;
  DEBUG(("Got connection"));
  client = client_create(conn_fd, NULL);
  DEBUG(("Communicating"));
  success = communicate(server, client);
  DEBUG(("Communication %s", success ? "succesful" : "failed"));
  client_destroy(client);
  return success;
}

/* When communicate() is done, the client context should be removed from
   the list with this call. */
void server_destroy_client(Server server, Client client)
{

}

/* Functions used to communicate by default. */
int client_default_read(Client client, char *buf, size_t bytes, void *context)
{
  return read(client->conn_fd, buf, bytes);
}

Boolean client_default_write(Client client, char *buf, size_t bytes,
                             void *context)
{
  int written_bytes = write(client->conn_fd, buf, bytes);
  fsync(client->conn_fd);
  return bytes == written_bytes;
}

Client client_create(int conn_fd, ClientCreateParams params)
{
  Client client = xcalloc(1, sizeof(*client));
  client->conn_fd = conn_fd;
  client->read = client_default_read;
  client->write = client_default_write;

  if (params)
    {
      if (params->client_read)
        {
          client->read = params->client_read;
          client->read_context = params->client_read_context;
        }

      if (params->client_write)
        {
          client->write = params->client_write;
          client->write_context = params->client_write_context;
        }
    }
  return client;
}

void client_destroy(Client client)
{
  if (!client)
    return;

  if (client->conn_fd >= 0)
    close(client->conn_fd);
  xfree(client);
}

/* Convenience wrappers. */
int client_read(Client client, char *buf, size_t bytes)
{
  return client->read(client, buf, bytes, client->read_context);
}

int client_write(Client client, char *buf, size_t bytes)
{
  return client->write(client, buf, bytes, client->write_context);
}

/* This will process the request.  The process function may be replaced
   with a function with similar semantics, but which will delay, wait
   for certain conditions, allocate huge amounts of memory, etc. */
Boolean process_line(Server server, Client client,
                     const char *line, char **reply_ret, char **errors_ret)
{
#define FIELD_WIDTH 20
  char param[FIELD_WIDTH], op[FIELD_WIDTH], arg1[FIELD_WIDTH],
    arg2[FIELD_WIDTH];
  DEBUG(("Processing line: %s", line));

  int ret = sscanf(line, "%19s %19s %19s %19s",
                   param,
                   op,
                   arg1,
                   arg2);
  if (ret != 4)
    {
      *errors_ret = xstrdup("Failed to parse line");
      return FALSE;
    }

  if (!strcmp(op, "+"))
    {
      int num1 = atoi(arg1);
      int num2 = atoi(arg2);
      *reply_ret = string_format(
                     "%s %s %s %s = %d", param, op, arg1, arg2,
                     num1 + num2);
    }
  else if (!strcmp(op, "LIST"))
    {
      /* Operands ignored, we just return the list of connected clients. */
      /* STUB: needs to be implemented */
    }
  else if (!strcmp(op, "NUMCLIENTS"))
    {
      /* Operands ignored, we just return the number of clients. */
      /* STUB: needs to be implemented */
    }
  else
    {
      *errors_ret = xstrdup("unknown op");
      return FALSE;
    }
  return TRUE;
}

Boolean communicate(Server server, Client client)
{
  char buf[256];
  int read_bytes = 0;

  while (TRUE)
    {
      if (read_bytes >= sizeof(buf) - 1)
        {
          warning("Protocol error, too long line");
          return FALSE;
        }
      int ret = client_read(client, &buf[read_bytes], 1);
      DEBUG(("Client read returned %d", ret));
      if (ret < 0)
        {
          warning("Comm channel in error: %m");
          return FALSE;
        }
      else if (ret == 0)
        {
          DEBUG(("Client in EOF"));
          if (read_bytes == 0)
            {
              return TRUE;
            }
          else
            {
              warning("Protocol error, leftovers in read buffer");
              return FALSE;
            }
        }
      else
        {
          char last_char = buf[read_bytes];

          if (last_char == '\n')
            {
              Boolean success;
              char *errors = NULL, *reply = NULL;
              buf[read_bytes] = '\0';
              success = process_line(server, client, buf, &reply, &errors);
              DEBUG(("Processing done: status: %d, reply: %s, errors: %s",
                     success, reply, errors));
              read_bytes = 0;
              if (!success)
                {
                  warning("processing failed: %s", errors);
                  xfree(errors);
                  xfree(reply);
                  return FALSE;
                }
              xfree(errors);
              {
                char *response = string_format("%s\n", reply);
                success = client_write(client, response, strlen(response));
                xfree(response);
              }
              if (!success)
                {
                  warning("Failed to send reply");
                  return FALSE;
                }
              DEBUG(("Client request processed"));
            }
          else
            {
              read_bytes++;
            }
        }
    }
}
