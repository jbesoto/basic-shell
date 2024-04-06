/**
 * @file shell.c
 *
 * @brief Basic Unix-like Shell Implementation
 *
 * @author Juan Diego Becerra (jdb9056@nyu.edu)
 * @date   Apr 5, 2024
 */

#include "shell.h"

int status = 0;

int main(void) {
  char cmdline[kInputMax];
  char *ps1 = getenv("PS1");

  while (1) {
    ps1 ? printf("%s ", ps1) : ExpandPromptString();

    if (!fgets(cmdline, kInputMax, stdin)) {
      PrintError("%s\n", strerror(errno));
      continue;
    }
    // Remove newline character
    cmdline[strlen(cmdline) - 1] = '\0';

    DynamicArray *da_args = TokenizeCommandLine(cmdline);
    if (!da_args) {
      PrintError("failed to tokenize command line: %s\n", strerror(errno));
      continue;
    }

    char **args = (char **)da_args->data;
    if (strcmp(args[0], "cd") == 0) {
      if (da_args->len < 2) {
        fprintf(stderr, "cd: missing operand\n");
        FreeDynamicArray(da_args);
        status = 1;
        continue;
      }

      char *pathname = args[1];
      if (chdir(pathname) < 0) {
        fprintf(stderr, "cd: %s: %s\n", strerror(errno), pathname);
        FreeDynamicArray(da_args);
        status = 1;
        continue;
      }
    } else if (strcmp(args[0], "exit") == 0) {
      FreeDynamicArray(da_args);
      exit(status);
    }

    FreeDynamicArray(da_args);
  }
}

void ExpandPromptString(void) {
  struct passwd *pwd;
  char cwd[kPathMax];
  char hostname[kHostnameMax];

  puts("");
  const char *ps = kPromptString;
  while (*ps) {
    if (*ps == '\\') {
      ps++;  // Skip backslash
      switch (*ps) {
        case 'u':
          pwd = getpwuid(getuid());
          if (!pwd) {
            break;
          }
          printf("%s", pwd->pw_name);
          break;

        case 'b':
          if (!getcwd(cwd, sizeof(cwd))) {
            break;
          }
          printf("%s", basename(cwd));
          break;

        case 'h':
          if (gethostname(hostname, sizeof(hostname)) < 0) {
            break;
          }
          printf("%s", hostname);
          break;

        default:
          break;
      }
    } else {
      printf("%c", *ps);
    }
    ps++;
  }
  printf("%s ", (getuid() == kRootUID) ? "#" : "$");
  fflush(stdout);
}

// This implementation does not treat text wrapped in quotes as single token
DynamicArray *TokenizeCommandLine(char *cmdline) {
  DynamicArray *da_tokens = InitDynamicArray(kDefaultArraySize, sizeof(char *));
  if (!da_tokens) {
    return NULL;
  }

  for (;; cmdline = NULL) {
    char *token = strtok(cmdline, " ");
    if (!token) {
      break;
    }

    if (AppendElement(da_tokens, &token) < 0) {
      FreeDynamicArray(da_tokens);
      return NULL;
    }
  }

  return da_tokens;
}

Process *InitProcess(void) {
  Process *proc = malloc(sizeof(Process));
  if (!proc) {
    return NULL;
  }

  if ((proc->o_stdin = dup(STDIN_FILENO)) < 0) {
    goto init_proc_error;
  }
  if ((proc->o_stdout = dup(STDOUT_FILENO)) < 0) {
    close(proc->stdin);
    goto init_proc_error;
  }
  if ((proc->o_stderr = dup(STDERR_FILENO)) < 0) {
    close(proc->stdin);
    close(proc->stdout);
    goto init_proc_error;
  }

  // Mark standard streams as unredirected
  proc->stdin = -1;
  proc->stdout = -1;
  proc->stderr = -1;

  return proc;

init_proc_error:
  free(proc);
  return NULL;
}

void _PrintError(const char *func, int line, const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "[%s: %d] shell: ", func, line);
  vfprintf(stderr, format, args);

  va_end(args);
}

/**
 * @brief Initializes a dynamic array with specified size and type size.
 *
 * Failure to allocate the data in the dynamic array results in freeing up
 * the memory allocated for the `DynamicArray` struct.
 *
 * @param size      Initial number of elements the array can hold.
 * @param type_size Size of each element in the array.
 *
 * @return Pointer to the initialized dynamic array if sucesseful. Otherwise,
 *         returns NULL if memory allocation fails at any point and `errno` is
 *         set appropriately.
 *
 * @note Allocated dynamic array can be freed using `FreeDynamicArray()`.
 */
DynamicArray *InitDynamicArray(size_t size, size_t type_size) {
  DynamicArray *da = malloc(sizeof(DynamicArray));
  if (!da) {
    return NULL;
  }

  da->data = malloc(size * type_size);
  if (!da->data) {
    FreeDynamicArray(da);
    return NULL;
  }
  da->size = size;
  da->len = 0;
  da->type_size = type_size;

  return da;
}

/**
 * @brief Frees the memory allocated for a dynamic array.
 *
 * Dynamic array pointers set to NULL are ignored.
 *
 * @param da Pointer to the dynamic array to be freed.
 */
void FreeDynamicArray(DynamicArray *da) {
  if (da) {
    if (da->data) {
      free(da->data);
    }
    free(da);
  }
}

/**
 * @brief Appends an element to the dynamic array.
 *
 * If the array is already full, it attempts to resize the array to double its
 * current size.
 *
 * @param da   Pointer to the dynamic array where the element will be added.
 * @param elem Pointer to the element to be added to the dynamic array.
 *
 * @return 0 if the element is successfully added. Otherwise, returns -1 if
 *         dynamic array cannot be resized and `errno` is set appropriately.
 */
int AppendElement(DynamicArray *da, void *elem) {
  if (da->size == da->len) {
    if (!ResizeDynamicArray(da, da->size * 2)) {
      return -1;
    }
    da->size *= 2;
  }
  // Using char* enables copying byte-to-byte
  memcpy((char *)da->data + (da->len * da->type_size), elem, da->type_size);
  da->len++;

  return 0;
}

/**
 * @brief Resizes a dynamic array to a new size.
 *
 * If resizing is successful, updates the array's size and data pointer to the
 * reallocated memory block.
 *
 * @param da       Pointer to the dynamic array to be resized.
 * @param new_size The new size for the dynamic array.
 *
 * @return 0 if the dynamic array was successfully resized. On error, -1 is
 *         returned and `errno` is set appropriately,
 */
int ResizeDynamicArray(DynamicArray *da, size_t new_size) {
  void *resized_data = reallocarray(da->data, new_size, da->type_size);
  if (!resized_data) {
    return -1;
  }
  da->data = resized_data;
  da->size = new_size;

  return 0;
}
