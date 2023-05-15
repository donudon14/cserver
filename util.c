/*
 * Utility functions for the project.
 */
#define _GNU_SOURCE
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>

static OutputMode output_mode = OM_NORMAL;

OutputMode get_output_mode()
{
  return output_mode;
}

void set_output_mode(OutputMode given_output_mode)
{
  output_mode = given_output_mode;
}

char *string_format(const char *format, ...)
{
  char *message = NULL;
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vasprintf(&message, format, ap);
  va_end(ap);
  if (ret < 0)
    fatal("Failed string format");
  return message;
}

void debug(const char *format, ...)
{
  va_list ap;
  int ret_val;
  char *string = NULL;
  va_start(ap, format);
  ret_val = vasprintf(&string, format, ap);
  va_end(ap);
  if (ret_val >= 0)
    {
      fprintf(stderr, "%s\n", string);
      xfree(string);
    }
}

void warning(const char *format, ...)
{
  va_list ap;
  int ret_val;
  char *string = NULL;
  va_start(ap, format);
  ret_val = vasprintf(&string, format, ap);
  va_end(ap);
  if (ret_val >= 0)
    {
      fprintf(stderr, "WARNING: %s\n", string);
      xfree(string);
    }
}

void default_fatal_handler(const char *msg, void *context)
{
  fprintf(stderr, "FATAL: %s\n", msg ? msg : "(null)");
  abort();
  /* In case some humorist has trapped SIGABORT. */
  exit(2);
}

static void (*fatal_handler)(const char *msg, void *context) =
  default_fatal_handler;
static void *fatal_context = NULL;

void register_fatal_handler(void (*fatal_cb)(const char *msg, void *context),
                            void *context)
{
  fatal_handler = fatal_cb;
  fatal_context = context;
}

void fatal(const char *fmt, ...)
{
  va_list ap;
  int ret_val;
  char *string = NULL;
  va_start(ap, fmt);
  ret_val = vasprintf(&string, fmt, ap);
  va_end(ap);
  if (ret_val < 0)
    string = NULL;

  fatal_handler(string, fatal_context);
  xfree(string);
}

void xfree(void *ptr)
{
  if (!ptr)
    return;
  free(ptr);
}

void *xcalloc(size_t num_items, size_t size)
{
  void *ptr = calloc(num_items, size);
  if (!ptr)
    fatal("memory allocation failed.");
  return ptr;
}

char *xstrdup(const char *ptr)
{
  char *newptr = NULL;
  if (!ptr)
    return NULL;
  newptr = strdup(ptr);
  if (!ptr)
      fatal("memory allocation failed.");

  return newptr;
}

int create_local_listener(const char *listener_path)
{
  int ret_sock = -1, ret_val;
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un saddr = {0};

  if (sock_fd < 0)
    {
      warning("Could not allocate socket.");
      goto error;
    }

  strncpy(saddr.sun_path, listener_path, sizeof(saddr.sun_path));
  saddr.sun_family = AF_UNIX;

  ret_val = bind(sock_fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (ret_val < 0)
    {
      warning("Could not allocate socket: %m");
      goto error;
    }

  ret_val = listen(sock_fd, 100);
  if (ret_val < 0)
    {
      warning("Could not start listening on socket: %m");
      goto error;
    }

  /* Success. */
  ret_sock = sock_fd;
  sock_fd = -1;

 error:

  if (sock_fd >= 0)
    close(sock_fd);

  return ret_sock;
}

Boolean thread_create(void* (*thread_func)(void *context), void *context)
{
  pthread_t thread;
  pthread_attr_t thread_attrs;
  int ret;
  Boolean ret_val = FALSE;

  ret = pthread_attr_init(&thread_attrs);
  if (ret != 0)
    {
      warning("Failed to initialize thread attributes: code %d", ret);
      return FALSE;
    }

  /* The new threads should make do with 128 kiB of stack.  Default
     allocates too much for *many* threads. */
  ret = pthread_attr_setstacksize(&thread_attrs, 128 * 1024);
  if (ret != 0)
    {
      warning("Failed to set stacksize: code %d", ret);
      return FALSE;
    }

  ret = pthread_create(&thread, &thread_attrs, thread_func, context);
  if (ret != 0)
    {
      warning("Failed to create thread: code %d", ret);
      goto error;
    }

  ret = pthread_detach(thread);
  if (ret != 0)
    {
      warning("Failed to detach thread: code %d", ret);
      /* Should never happen. */
      goto error;
    }

  ret_val = TRUE;
 error:
  pthread_attr_destroy(&thread_attrs);
  return ret_val;
}

typedef struct MutexRec
{
  pthread_mutex_t mutex;
  Boolean locked;
} MutexStruct;

Mutex mutex_create()
{
  Mutex mutex = xcalloc(1, sizeof(*mutex));
  int ret;

  ret = pthread_mutex_init(&mutex->mutex, NULL);
  if (ret != 0)
    fatal("Failed to initialize mutex: code %d", ret);

  return mutex;
}

void mutex_destroy(Mutex mutex)
{
  int ret;

  if (!mutex)
    return;

  if (mutex->locked)
    {
      fatal("Destroying a locked mutex");
      /* For tests.*/
      return;
    }
  ret = pthread_mutex_destroy(&mutex->mutex);
  if (ret != 0)
    warning("Failed to destroy mutex: code %d", ret);
  xfree(mutex);
}

void mutex_lock(Mutex mutex)
{
  int ret = pthread_mutex_lock(&mutex->mutex);
  if (ret != 0)
    fatal("Failed to lock mutex: code %d", ret);
  mutex->locked = TRUE;
}

void mutex_unlock(Mutex mutex)
{
  int ret;
  mutex->locked = FALSE;
  ret = pthread_mutex_unlock(&mutex->mutex);
  if (ret != 0)
    fatal("Failed to unlock mutex: code %d", ret);
}

typedef struct ConditionRec
{
  pthread_cond_t cond;
} ConditionStruct;

Condition condition_create(void)
{
  Condition cv = xcalloc(1, sizeof(*cv));
  int ret = pthread_cond_init(&cv->cond, NULL);
  if (ret != 0)
    fatal("Failed to create condition variable");
  return cv;
}

void condition_signal(Condition cv)
{
  int ret = pthread_cond_signal(&cv->cond);
  if (ret != 0)
    fatal("Failed to signal condition: code %d", ret);
}

void condition_broadcast(Condition cv)
{
  int ret = pthread_cond_broadcast(&cv->cond);
  if (ret != 0)
    fatal("Failed to signal condition: code %d", ret);
}

void condition_wait(Condition cv, Mutex mutex)
{
  mutex->locked = FALSE;
  int ret = pthread_cond_wait(&cv->cond, &mutex->mutex);
  if (ret != 0)
    fatal("Failed to signal condition: code %d", ret);
  mutex->locked = TRUE;
}

void condition_destroy(Condition cv)
{
  int ret;

  if (!cv)
    return;
  ret = pthread_cond_destroy(&cv->cond);
  if (ret != 0)
    fatal("Failed to destroy condition variable");
  xfree(cv);
}
