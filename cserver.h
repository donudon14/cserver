/*
 * The API for the computation server.
 */

#ifndef _CSERVER_H_
#define _CSERVER_H_

#include "util.h"

/***************************** API definition. ******************************/

typedef struct ServerRec * Server;

/* Create the server object. */
Server server_create(void);
/* Calling this is only legal if there are no ongoing requests for the
   server. */
void server_destroy(Server server);

void server_shutdown(Server server);

/* Returns TRUE if shutdown has been called for the server, FALSE otherwise. */
Boolean server_shutdown_requested(Server server);

Boolean server_accept_connection(Server server, int conn_fd, char **errors_ret);

typedef struct ClientRec *Client;

typedef struct ClientCreateParamsRec
{
  /* Can be used to override the IO functionality. */
  int (*client_read)(Client client, char *buf, size_t bytes, void *context);
  void *client_read_context;

  /* Return TRUE if everything was successfully written, FALSE otherwise. */
  Boolean (*client_write)(Client client, char *buf, size_t bytes,
                          void *context);
  void *client_write_context;

} ClientCreateParamsStruct, *ClientCreateParams;

Client client_create(int sock_fd, ClientCreateParams params);
void client_destroy(Client client);

/* Performs the actual protocol between the client and the server. */
Boolean communicate(Server server, Client client);

#endif  /* _CSERVER_H_ */
