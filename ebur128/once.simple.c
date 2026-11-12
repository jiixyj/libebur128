#include "once.h"

#warning "choose pthread for thread safety!"

static int g_once = 0;

void ebur128_once_call(void (*fn)(void)) {
  if (!g_once) {
    fn();
    g_once = 1;
  }
}
