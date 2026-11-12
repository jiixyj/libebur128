#include "once.h"

#include <stddef.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void (*ebur128_once_fn)(void);

typedef struct ebur128_once_context {
  ebur128_once_fn fn;
} ebur128_once_context;

static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK ebur128_once_callback(PINIT_ONCE init_once,
                                           PVOID parameter, PVOID* context) {
  ebur128_once_context* once_context = (ebur128_once_context*) parameter;

  (void) init_once;
  (void) context;
  once_context->fn();
  return TRUE;
}

void ebur128_once_call(ebur128_once_fn fn) {
  ebur128_once_context context;

  context.fn = fn;
  (void) InitOnceExecuteOnce(&g_once, ebur128_once_callback, &context, NULL);
}
