/*
 * A simple standalone unit test framework.
 *
 * You should add your tests in the "Test functions" block below.
 *
 */

#include "util.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "cserver.h"

/***************************** Test functions. ******************************/

/* XXX test setup and teardown missing. */

#define TEST_RET static Boolean

TEST_RET test_create_listener(char **errors_ret)
{
  int sock_fd = create_local_listener("foo.sock"), sock_fd2 = -1;
  Boolean ret_val = FALSE;
  if (sock_fd < 0)
    {
      *errors_ret = xstrdup("listener allocation failed");
      goto error;
    }

  sock_fd2 = create_local_listener("foo.sock");
  if (sock_fd2 != -1)
    {
       *errors_ret = xstrdup("listener allocation unexpectedly succeeded");
       goto error;
    }

  ret_val = TRUE;
 error:
  if (sock_fd >= 0)
    close(sock_fd);
  if (sock_fd2 >= 0)
    close(sock_fd2);
  unlink("foo.sock");
  return ret_val;
}

TEST_RET test_server_shutdown(char **errors_ret)
{
  Boolean ret_val = FALSE;
  Server server = server_create();
  if (!server)
    {
       *errors_ret = xstrdup("server creation failed");
       goto error;
    }

  if (server_shutdown_requested(server) == TRUE)
    {
       *errors_ret = xstrdup("shutdown should not yet be requested");
       goto error;
    }

  server_shutdown(server);

  if (!server_shutdown_requested(server))
    {
       *errors_ret = xstrdup("shutdown should have been requested");
       goto error;
    }

  server_destroy(server);
  ret_val = TRUE;
 error:
  return ret_val;
}

static void count_fatals(const char *message, void *context)
{
  int *count = context;

  DEBUG(("Received fatal: %s", message));
  (*count)++;
}

TEST_RET test_mutex(char **errors_ret)
{
  Mutex mutex = mutex_create();
  Boolean ret_val = FALSE;
  int fatal_count = 0;

  if (!mutex)
    {
       *errors_ret = xstrdup("mutex should have been allocated");
       goto error;
    }

  mutex_lock(mutex);

  register_fatal_handler(count_fatals, &fatal_count);
  mutex_destroy(mutex);

  if (fatal_count <= 0)
    {
      *errors_ret = xstrdup("destroying a locked mutex should be a "
                            "fatal error");
      goto error;
    }

  register_fatal_handler(default_fatal_handler, NULL);

  mutex_unlock(mutex);
  mutex_destroy(mutex);

  ret_val = TRUE;
 error:
  register_fatal_handler(default_fatal_handler, NULL);
  return ret_val;
}

TEST_RET test_null_mutex_destroy(char **errors_ret)
{
  /* It should be just fine to destroy a NULL mutex. */
  mutex_destroy(NULL);
  return TRUE;
}

TEST_RET test_condition(char **errors_ret)
{
  Condition cv = condition_create();
  Boolean ret_val = FALSE;

  if (!cv)
    {
       *errors_ret = xstrdup("condition variable should have been allocated");
       goto error;
    }

  condition_destroy(cv);

  ret_val = TRUE;
 error:
  return ret_val;
}

TEST_RET test_null_condition_destroy(char **errors_ret)
{
  /* It should be fine to destroy a NULL condition. */
  condition_destroy(NULL);
  return TRUE;
}

static void *noop(void *context)
{
  return NULL;
}

TEST_RET test_thread_noop(char **errors_ret)
{
  Boolean ret_val = FALSE, success;

  success = thread_create(noop, NULL);
  if (!success)
    {
      *errors_ret = xstrdup("Failed to create thread");
      goto error;
    }

  ret_val = TRUE;
 error:
  return ret_val;
}

typedef struct ThreadTestCtxRec
{
  Mutex mutex;
  Condition cv;
  Boolean visited;
} ThreadTestCtxStruct, *ThreadTestCtx;

void *toggle_handler(void *context)
{
  ThreadTestCtx test_ctx = context;
  usleep(5 * 1000);

  mutex_lock(test_ctx->mutex);
  test_ctx->visited = TRUE;
  condition_signal(test_ctx->cv);
  mutex_unlock(test_ctx->mutex);
  return NULL;
}

TEST_RET test_thread(char **errors_ret)
{
  ThreadTestCtxStruct test_ctx[1] = { { 0 } };
  Boolean success;
  Boolean ret_val = FALSE;

  test_ctx->mutex = mutex_create();
  test_ctx->cv = condition_create();

  success = thread_create(toggle_handler, test_ctx);
  if (!success)
    {
      *errors_ret = xstrdup("Failed to create thread");
      goto error;
    }

  mutex_lock(test_ctx->mutex);
  while (!test_ctx->visited)
    condition_wait(test_ctx->cv, test_ctx->mutex);
  mutex_unlock(test_ctx->mutex);

  ret_val = TRUE;
 error:
  mutex_destroy(test_ctx->mutex);
  condition_destroy(test_ctx->cv);
  return ret_val;
}

typedef struct CommunicationReadTestCtxRec
{
  int ret_val;
  char *ret_buffer;
} CommunicationReadTestCtxStruct, *CommunicationReadTestCtx;

static int mock_read(Client client, char *buf, size_t bytes, void *context)
{
  CommunicationReadTestCtx test_ctx = context;
  if (!test_ctx->ret_buffer)
    {
      return test_ctx->ret_val;
    }
  else
    {
      /* Return buffer is only written once. */
      char *ret_buffer = test_ctx->ret_buffer;
      int bytes_left;
      bytes_left = strlen(ret_buffer);
      if (bytes_left < bytes)
        bytes = bytes_left;

      strncpy(buf, ret_buffer, bytes);

      if (bytes_left == bytes)
        {
          test_ctx->ret_buffer = NULL;
          test_ctx->ret_val = 0;
        }
      else
        {
          /* Move the rest to the front.  +1 is for the NIL byte. */
          memmove(test_ctx->ret_buffer, &test_ctx->ret_buffer[bytes],
                  bytes_left - bytes + 1);
        }
      return bytes;
    }
}

typedef struct CommunicationWriteTestCtxRec
{
  Boolean ret_val;
  char ret_buffer[256];
} CommunicationWriteTestCtxStruct, *CommunicationWriteTestCtx;


static Boolean mock_write(Client client, char *buf, size_t bytes, void *context)
{
  CommunicationWriteTestCtx test_ctx = context;
  if (!test_ctx->ret_val)
    {
      return FALSE;
    }
  else
    {
      strncpy(test_ctx->ret_buffer, buf, bytes);
      return TRUE;
    }
}

#define LONG_BUFFER "ggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"\
  "ggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"\
  "ggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"\
  "ggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"\
  "ggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"

TEST_RET test_communicate(char **errors_ret)
{
  Server server = server_create();
  Boolean ret_val = FALSE;
  ClientCreateParamsStruct params[1] = { { 0 } };
  Client client;
  CommunicationReadTestCtxStruct read_test_ctx[1] = { { 0 } };
  CommunicationWriteTestCtxStruct write_test_ctx[1] = { { 0 } };

  params->client_read = mock_read;
  params->client_read_context = read_test_ctx;

  params->client_write = mock_write;
  params->client_write_context = write_test_ctx;

  write_test_ctx->ret_val = TRUE;

  client = client_create(-1, params);

  read_test_ctx->ret_val = -1;

  if (communicate(server, client) != FALSE)
    {
       *errors_ret = xstrdup("communication should have failed");
       goto error;
    }

  read_test_ctx->ret_val = 0;
  if (communicate(server, client) != TRUE)
    {
       *errors_ret = xstrdup("communication should succeed");
       goto error;
    }

  read_test_ctx->ret_buffer = xstrdup("foo"); /* No newline. */
  if (communicate(server, client) != FALSE)
    {
       *errors_ret = xstrdup("communication should fail because of crud");
       goto error;
    }
  xfree(read_test_ctx->ret_buffer);
  read_test_ctx->ret_buffer = xstrdup(LONG_BUFFER);
  if (communicate(server, client) != FALSE)
    {
       *errors_ret = xstrdup("communication should fail because of "
                             "too long line");
       goto error;
    }

  xfree(read_test_ctx->ret_buffer);
  read_test_ctx->ret_buffer = xstrdup("100 + 5 5\n");
  if (communicate(server, client) != TRUE)
    {
       *errors_ret = xstrdup("communication should succeed");
       goto error;
    }

  if (strcmp(write_test_ctx->ret_buffer, "100 + 5 5 = 10\n") != 0)
    {
       *errors_ret = xstrdup("did not receive expected return buffer");
       goto error;
    }

  xfree(read_test_ctx->ret_buffer);
  read_test_ctx->ret_buffer = xstrdup("100 ' 5 5\n");
  if (communicate(server, client) != FALSE)
    {
       *errors_ret = xstrdup("communication should fail with invalid op");
       goto error;
    }

  xfree(read_test_ctx->ret_buffer);
  read_test_ctx->ret_buffer = xstrdup("100 + 5\n");
  if (communicate(server, client) != FALSE)
    {
       *errors_ret = xstrdup("communication should fail with invalid args");
       goto error;
    }

  client_destroy(client);
  server_destroy(server);

  ret_val = TRUE;
 error:
  xfree(read_test_ctx->ret_buffer);
  return ret_val;
}

/* Add your tests here. */

/***************************** Test framework. ******************************/

#define FUN(fun)                                \
  { #fun, fun }

struct {
  char *name;
  Boolean (*func)(char **);
  int serial_number;
} test_funcs[] =
  {
    FUN(test_create_listener),
    FUN(test_server_shutdown),

    FUN(test_mutex),
    FUN(test_null_mutex_destroy),
    FUN(test_condition),
    FUN(test_null_condition_destroy),
    FUN(test_thread_noop),
    FUN(test_thread),

    FUN(test_communicate),

    { NULL, NULL }
  };


struct option long_options[] =
  {
    { "debug", FALSE, NULL, 'd' },
    { "verbose", FALSE, NULL, 'v' },
    { "quiet",   FALSE, NULL, 'q' },
    {NULL, 0, 0, 0}
  };

int main(int argc, char **argv)
{
  int tests_run = 0, failed_tests = 0;
  int ii, opt;

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

  /* Run tests. */
  for (ii = 0; test_funcs[ii].name; ii++)
    {
      char *errors = NULL;
      Boolean ret;
      ret = (*test_funcs[ii].func)(&errors);
      if (!ret)
        {
          if (get_output_mode() != OM_QUIET)
            {
              fprintf(stderr, "\n=== Failed %s ===\n", test_funcs[ii].name);
              if (errors)
                fprintf(stderr, "errors: %s\n", errors);
            }
          else
            {
              fprintf(stderr, "F");
            }
          failed_tests++;
        }
      else
        {
          fprintf(stderr, ".");
        }
      xfree(errors);
      tests_run++;
    }

  {
    Boolean success = failed_tests == 0 ? TRUE : FALSE;
    fprintf(stderr,
            "\n%s: Ran %d test%s, with %d failure%s.\n.",
            success ? "OK" : "FAIL",
            tests_run, tests_run == 1 ? "" : "s",
            failed_tests, failed_tests == 1 ? "" : "s");
    return success ? 0 : 1;
  }
}
