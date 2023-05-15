/* The application entrypoint. */
#define _GNU_SOURCE
#include "cserver.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <getopt.h>

struct option long_options[] =
  {
    { "debug", FALSE, NULL, 'd' },
    { "verbose", FALSE, NULL, 'v' },
    { "quiet",   FALSE, NULL, 'q' },
    {NULL, 0, 0, 0}
  };

int main(int argc, char **argv)
{
  int exit_value = 1;
  int sock_fd;
  Server server;
  const char *listen_sock = "/tmp/cserver.sock";
  int opt;

  while ((opt = getopt_long(argc, argv, "vqd", long_options, NULL)) != -1)
    {
      switch (opt)
        {
        case 'd':
          set_output_mode(OM_DEBUG);
          break;

        case 'v':
          set_output_mode(OM_VERBOSE);
          break;

        case 'q':
          set_output_mode(OM_QUIET);
          break;
        }
    }

  /* Cleanup leftover file from previous run. */
  unlink(listen_sock);
  sock_fd = create_local_listener(listen_sock);
  if (sock_fd < 0)
    {
      warning("Failed to create listener.");
      goto error;
    }

  server = server_create();
  if (!server)
    {
      warning("Failed to create server.");
      goto error;
    }

  while (!server_shutdown_requested(server))
    {
      Boolean success;
      int conn_fd;
      struct sockaddr saddr = {0};
      socklen_t saddr_len = sizeof(saddr);
      char *errors = NULL;

      conn_fd = accept(sock_fd, &saddr, &saddr_len);
      if (conn_fd < 0)
        {
          warning("Failed to low-level accept(): %m");
          continue;
        }
      success = server_accept_connection(server, conn_fd, &errors);
      if (!success)
        {
          warning("Failed to accept connection: %s", errors);
          close(conn_fd);
        }
      xfree(errors);
    }

  exit_value = 0;
 error:
  if (sock_fd >= 0)
    close(sock_fd);
  return exit_value;
}
