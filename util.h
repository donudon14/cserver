#ifndef _UTIL_H_
#define _UTIL_H_
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>

/**************************** Utility functions. ****************************/
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef unsigned int Boolean;

/* Memory allocation. */

/* free() the pointer safely.  This is a no-op if `ptr' is NULL. */
void xfree(void *ptr);

/* Safely duplicate a given string.  fatal() is called on
   memory-allocation failure. */
char *xstrdup(const char *ptr);

/* Safe allocation for `num_objects' objects.  fatal() is called on
   memory-allocation failure.  */
void *xcalloc(size_t num_objects, size_t size);

/* Inter-process communication. */

/* Create a local listener to given `listener_path'.  Return -1 on
   failure.  A returned non-negative integer is the allocated socket
   file descriptor. */
int create_local_listener(const char *listener_path);

/* Reporting functions. */

/* Output mode. */
typedef enum { OM_QUIET, OM_NORMAL, OM_VERBOSE, OM_DEBUG } OutputMode;

OutputMode get_output_mode();
void set_output_mode(OutputMode output_mode);

char *string_format(const char *fmt, ...);

#define DEBUG(varcall)                                          \
do {                                                            \
  if (get_output_mode() == OM_DEBUG)                            \
    {                                                           \
      char * msg = string_format varcall;                       \
      debug("DEBUG: %s:%d: %s", __FILE__, __LINE__, msg);       \
      xfree(msg);                                               \
    }                                                           \
 } while(0)

/* Emit a debug message. */
void debug(const char *fmt, ...);

/* Emit a warning message. */
void warning(const char *fmt, ...);

/* Emit an error message and forcibly exit. */
void fatal(const char *fmt, ...);

/* For testing, NEVER for production code. */
void default_fatal_handler(const char *message, void *context);

/* This will replace the default fatal behaviour to one which will not
   terminate.  This allows to test function, whose errors are terminal.
   MUST NOT be used otherwise.  Behaviour can be reset by registering
   the default_fatal_handler() afterwards. */
void register_fatal_handler(void (*fatal_handler)(const char *message,
                                                  void *context),
                            void *context);

/* Thread primitives. */
typedef struct MutexRec *Mutex;
typedef struct ConditionRec *Condition;

Mutex mutex_create(void);
void mutex_destroy(Mutex mutex);

/* Obtain mutex. */
void mutex_lock(Mutex mutex);
/* Release mutex. */
void mutex_unlock(Mutex mutex);

Condition condition_create(void);
void condition_destroy(Condition cv);

void condition_signal(Condition cv);
void condition_broadcast(Condition cv);
/* Wait for the condition variable to be signaled or broadcast.  This
   function will also atomically unlock the given mutex. */
void condition_wait(Condition cv, Mutex mutex);

/* Start a detached thread with the given function and argument.
   Returns TRUE if the thread was started successfully, FALSE otherwise.
   Notice that the number of threads that can be created may wary
   drastically from system to system.  The return value from the
   thread_func is ignored and should always be NULL. */
Boolean thread_create(void *(*thread_func)(void *context), void *context);

#endif  /* _UTIL_H_ */
