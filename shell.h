#ifndef SHELL_H_
#define SHELL_H_

#include <errno.h>
#include <libgen.h>  // basename
#include <pwd.h>     // getpwuid
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PrintError(format, ...) _PrintError(__func__, __LINE__, format, ##__VA_ARGS__)

const size_t kInputMax = 1024;
const size_t kPathMax = 512;
const char *kPromptString = "\\u@\\h : \\b\n";
const size_t kHostnameMax = 64;
const unsigned int kRootUID = 0;

void ExpandPromptString(void);
void _PrintError(const char *func, int line, const char *format, ...);

#endif  // SHELL_H_