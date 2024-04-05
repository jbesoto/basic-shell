#ifndef SHELL_H_
#define SHELL_H_

#include <stdio.h>
#include <stdarg.h>

#define PrintError(format, ...) _PrintError(__func__, __LINE__, format, ##__VA_ARGS__)

void _PrintError(const char *func, int line, const char *format, ...);

#endif  // SHELL_H_