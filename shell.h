#ifndef SHELL_H_
#define SHELL_H_

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define PrintError(format, ...) \
  _PrintError(__func__, __LINE__, format, ##__VA_ARGS__)

typedef struct {
  void *data;
  size_t size;
  size_t len;
  size_t type_size;
} DynamicArray;

typedef struct {
  char *cmd;
  char **args;
  int o_stdin, o_stdout, o_stderr;
  int stdin, stdout, stderr;
} Process;

typedef enum {
  kRedirectIn,
  kRedirectOut,
  kRedirectErr,
  kRedirectOutErr,
  kRedirectAppend,
  kNone
} RedirectType;

const size_t kDefaultArraySize = 16;
const size_t kInputMax = 1024;
const size_t kPathMax = 512;
const char *kPromptString = "\\u@\\h : \\b\n";
const size_t kHostnameMax = 64;
const unsigned int kRootUID = 0;

// Shell Functions
void ExpandPromptString(void);
Process *InitProcess(void);
DynamicArray *TokenizeCommandLine(char *cmdline);
int SetupRedirection(Process *proc, int newfd, RedirectType rtype);
RedirectType GetRedirectType(const char *op);
int CleanupRedirection(Process *proc);

// Dynamic Array Methods
int AppendElement(DynamicArray *da, void *elem);
void FreeDynamicArray(DynamicArray *da);
DynamicArray *InitDynamicArray(size_t size, size_t type_size);
int ResizeDynamicArray(DynamicArray *da, size_t new_size);

// Utility Functions
void _PrintError(const char *func, int line, const char *format, ...);

#endif  // SHELL_H_