#ifndef ONCE_H_
#define ONCE_H_

/*
 * Calls `fn` exactly once per process, even if multiple threads are present.
 * `fn` must always be the same function.
 */
void ebur128_once_call(void (*fn)(void));

#endif
