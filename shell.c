/**
 * @file shell.c
 *
 * @brief Basic Unix-like Shell Implementation
 *
 * @author Juan Diego Becerra (jdb9056@nyu.edu)
 * @date   Apr 5, 2024
 */

#include "shell.h"

int main(void) {
  return 0; 
}

void _PrintError(const char *func, int line, const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "[%s: %d] shell: ", func, line);
  vfprintf(stderr, format, args);
  
  va_end(args);
}
