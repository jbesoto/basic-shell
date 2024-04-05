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

typedef struct {
  void* data;
  size_t size;
  size_t len;
  size_t type_size;
} DynamicArray;

const size_t kInputMax = 1024;
const size_t kPathMax = 512;
const char *kPromptString = "\\u@\\h : \\b\n";
const size_t kHostnameMax = 64;
const unsigned int kRootUID = 0;

// Shell Functions
void ExpandPromptString(void);

// Dynamic Array Methods
int AppendElement(DynamicArray* da, void* elem);
void FreeDynamicArray(DynamicArray* da);
DynamicArray* InitDynamicArray(size_t size, size_t type_size);
int ResizeDynamicArray(DynamicArray* da, size_t new_size);

// Utility Functions
void _PrintError(const char *func, int line, const char *format, ...);

#endif  // SHELL_H_