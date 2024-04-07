/**
 * @file shell.c
 *
 * @brief Basic Unix-like Shell Implementation
 * 
 * This shell implementation offers a minimalist environment for executing
 * commands similar to traditional Unix shells. Features include:
 *
 * - I/O Redirection: Handles `<`, `>`, `>>`, `2>`, and `&>` for redirecting
 *   standard input, output, error streams, and appending to files. Redirection
 *   symbols are expected to be surrounded by whitespace and can appear
 *   anywhere in the command.
 * - Environment: Utilizes a customizable prompt string, defaulting to a simple
 *   format but can be overridden by the `PS1` environment variable. Special
 *   characters in the prompt string are treated as normal text.
 * - Built-in Commands: Supports basic navigation via `cd` and exiting the
 *   shell using `exit`.
 * - Shell Variable `$?`: Captures the exit status of the last executed command
 *   or the signal number (with bit 7 set) if the process terminated due to a
 *   signal.
 * - Signal Handling: Ignores `^C` (SIGINT) at the shell level, allowing
 *   interruption of child processes without exiting the shell.
 * 
 * @author Juan Diego Becerra (jdb9056@nyu.edu)
 * @date   Apr 5, 2024
 */

#include "shell.h"

/**
 * @brief Entry point of the shell program.
 *
 * Initializes the signal handling for SIGINT to ignore interruptions, and
 * enters a loop to read and execute commands from the user. It supports
 * built-in commands like 'cd' and 'exit', as well as external commands by
 * forking child processes. Handles command line tokenization, redirections,
 * and executes processes accordingly.
 */
int main(void) {
  char cmdline[kInputMax];
  char *ps1 = getenv("PS1");

  int status = 0;

  struct sigaction act, oldact;
  act.sa_handler = sigint_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(SIGINT, &act, &oldact) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  while (1) {
    ps1 ? printf("%s ", ps1) : ExpandPromptString();

    if (!fgets(cmdline, kInputMax, stdin)) {
      if (errno = EINTR) {
        clearerr(stdin);
        continue;
      }
      PrintError("%s\n", strerror(errno));
      continue;
    }

    // Remove newline character
    cmdline[strlen(cmdline) - 1] = '\0';

    if (*cmdline == '\0') {
      continue;
    }

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
      goto next_command;
    } else if (strcmp(args[0], "exit") == 0) {
      FreeDynamicArray(da_args);
      exit(status);
    }

    pid_t pid = fork();
    if (pid < 0) {
      FreeDynamicArray(da_args);
      PrintError("fork failed: %s\n", strerror(errno));
      status = 1;
      continue;
    } else if (pid == 0) {
      // Restore original disposition for SIGINT
      if (sigaction(SIGINT, &oldact, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
      }

      Process *proc = InitProcess();
      if (!proc) {
        FreeDynamicArray(da_args);
        PrintError("failed to initialize process: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      if (ParseCommand(proc, da_args, status) < 0) {
        free(proc);
        FreeDynamicArray(da_args);
        exit(EXIT_FAILURE);
      }

      execvp(proc->cmd, proc->args);
      if (errno == ENOENT) {
        PrintError("unrecognized command: %s\n", proc->cmd);
      } else {
        fprintf(stderr, "exec: %s\n", strerror(errno));
      }

      // exec failed
      FreeDynamicArray(da_args);
      CleanupRedirection(proc);
      free(proc);
      exit(EXIT_FAILURE);
    } else {
      int wstatus;
      if (waitpid(pid, &wstatus, 0) < 0) {
        FreeDynamicArray(da_args);
        PrintError("wait failed: %s\n", strerror(errno));
        continue;
      }

      if (WIFEXITED(wstatus)) {
        status = WEXITSTATUS(wstatus);
      } else if (WIFSIGNALED(wstatus)) {
        status = 128 + WTERMSIG(wstatus);
      } else {
        status = wstatus;
      }
    }

  next_command:
    FreeDynamicArray(da_args);
  }
}

/**
 * @brief Expands and prints the shell prompt string.
 *
 * Dynamically generates the shell prompt based on the current user, hostname,
 * and working directory, substituting placeholders with actual values. This
 * function is used to provide a customizable and informative shell prompt to
 * the user.
 */
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

/**
 * @brief Tokenizes the command line input.
 *
 * Splits the given command line input into tokens based on whitespace. It does
 * not treat text within quotes as a single token. The tokens are stored in a
 * dynamic array, which is returned to the caller.
 *
 * @param cmdline The command line input to be tokenized.
 *
 * @return A pointer to a DynamicArray containing the tokens, or NULL if an
 *         error occurs during tokenization.
 *
 * @note Text wrapped in quotes is not treated as a single token.
 */
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

/**
 * @brief Parses the command and its arguments for execution.
 *
 * Analyzes the tokens from the tokenized command line to set up the command
 * and its arguments for execution. It handles redirections by modifying file
 * descriptors as specified by the command. Also replaces the exit status
 * variable with the actual status.
 *
 * @param proc    Pointer to the Process structure to be filled with the
 *                command and its arguments.
 * @param da_args Pointer to the DynamicArray containing the tokenized command
 *                line.
 * @param status  The exit status of the last executed command, for
 *                substitution in the command line.
 *
 * @return 0 on success, or -1 if an error occurs, with errno set accordingly.
 */
int ParseCommand(Process *proc, DynamicArray *da_args, int status) {
  if (da_args->len == 0) {
    proc->cmd = "";
    proc->args = NULL;
    return 0;
  }

  char **args = (char **)da_args->data;
  for (size_t i = 0; i < da_args->len; i++) {
    RedirectType rtype = GetRedirectType(args[i]);
    if (rtype == kNone) {
      continue;
    }

    if (i + 1 >= da_args->len) {
      PrintError("missing redirection target\n");
      return -1;
    }

    int newfd;
    char *pathname = args[i + 1];
    switch (rtype) {
      case kRedirectIn:
        if ((newfd = open(pathname, O_RDONLY)) < 0) {
          PrintError("failed open: %s: %s", strerror(errno), pathname);
          return -1;
        }
        if (SetupRedirection(proc, newfd, rtype) < 0) {
          close(newfd);
          PrintError("failed redirection: '<': %s\n", strerror(errno));
          return -1;
        }
        break;

      case kRedirectOut:
        if ((newfd = open(pathname, O_CREAT | O_TRUNC | O_WRONLY, 0664)) < 0) {
          PrintError("failed open: %s: %s\n", pathname, strerror(errno));
          return -1;
        }
        if (SetupRedirection(proc, newfd, rtype) < 0) {
          close(newfd);
          PrintError("failed redirection: '>': %s\n", strerror(errno));
          return -1;
        }
        break;

      case kRedirectAppend:
        if ((newfd = open(pathname, O_CREAT | O_WRONLY | O_APPEND, 0664)) < 0) {
          PrintError("failed open: %s: %s\n", pathname, strerror(errno));
          return -1;
        }
        if (SetupRedirection(proc, newfd, rtype) < 0) {
          close(newfd);
          PrintError("failed redirection: '>>': %s\n", strerror(errno));
          return -1;
        }
        break;

      case kRedirectErr:
        if ((newfd = open(pathname, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
          PrintError("failed open: %s: %s\n", pathname, strerror(errno));
          return -1;
        }
        if (SetupRedirection(proc, newfd, rtype) < 0) {
          close(newfd);
          PrintError("failed redirection: '2>': %s\n", strerror(errno));
          return -1;
        }
        break;

      case kRedirectOutErr:
        if ((newfd = open(pathname, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
          PrintError("failed open: %s: %s\n", pathname, strerror(errno));
          return -1;
        }
        if (SetupRedirection(proc, newfd, rtype) < 0) {
          close(newfd);
          PrintError("failed redirection: '&>': %s\n", strerror(errno));
          return -1;
        }
        break;

      case kNone:
        continue;
    }
    close(newfd);

    // Remove current redirection operator and target file
    for (size_t count = 0; count < 2; count++) {
      for (size_t j = i; j < da_args->len - 1; j++) {
        args[j] = args[j + 1];
      }
      args[da_args->len - 1] = NULL;
      da_args->len--;
    }
    i--;  // Avoid skipping next redirection operator (if any)
  }

  proc->cmd = args[0];
  if (strcmp(proc->cmd, "echo") == 0) {
    ReplaceExitStatusVariable(da_args, status);
  }

  proc->args = args;
  proc->args[da_args->len] = NULL;

  return 0;
}

/**
 * @brief Initializes a new process structure.
 *
 * Allocates and initializes a new Process structure, setting up the original
 * standard file descriptors for later restoration. Used to manage redirections
 * and execution state of a command.
 *
 * @return A pointer to the newly allocated Process structure, or NULL if an
 *         error occurs during allocation or initialization.
 */
Process *InitProcess(void) {
  Process *proc = malloc(sizeof(Process));
  if (!proc) {
    return NULL;
  }

  // Mark streams as unused
  proc->in_fd = proc->out_fd = proc->err_fd = -1;

  if ((proc->orig_stdin = dup(STDIN_FILENO)) < 0) {
    goto init_proc_error;
  }
  if ((proc->orig_stdout = dup(STDOUT_FILENO)) < 0) {
    goto init_proc_error;
  }
  if ((proc->orig_stderr = dup(STDERR_FILENO)) < 0) {
    goto init_proc_error;
  }

  return proc;

init_proc_error:
  if (proc->orig_stdin >= 0) {
    close(proc->orig_stdin);
  }
  if (proc->orig_stdout >= 0) {
    close(proc->orig_stdout);
  }
  if (proc->orig_stderr >= 0) {
    close(proc->orig_stderr);  // being extra careful
  }
  free(proc);
  return NULL;
}

/**
 * @brief Sets up redirection for a process.
 *
 * Based on the specified redirection type, duplicates the new file descriptor
 * to replace the standard input, output, or error streams of the process.
 * Handles both individual and combined output/error redirections.
 *
 * @param proc  Pointer to the Process structure for which to set up
 *              redirection.
 * @param newfd The new file descriptor to use for the redirection.
 * @param rtype The type of redirection to be applied.
 *
 * @return 0 on success, or -1 on error with errno set accordingly.
 */
int SetupRedirection(Process *proc, int newfd, RedirectType rtype) {
  if (!proc) {
    return 0;
  }

  switch (rtype) {
    case kRedirectIn:
      if (proc->in_fd >= 0) {
        close(proc->in_fd);
      }
      if ((proc->in_fd = dup2(newfd, STDIN_FILENO)) < 0) {
        goto redirect_error;
      }
      break;

    case kRedirectAppend:
    case kRedirectOut:
      if (proc->out_fd >= 0) {
        close(proc->out_fd);
      }
      if ((proc->out_fd = dup2(newfd, STDOUT_FILENO)) < 0) {
        goto redirect_error;
      }
      break;

    case kRedirectErr:
      if (proc->err_fd >= 0) {
        close(proc->err_fd);
      }
      if ((proc->err_fd = dup2(newfd, STDERR_FILENO)) < 0) {
        goto redirect_error;
      }
      break;

    case kRedirectOutErr:
      if (proc->out_fd >= 0) {
        close(proc->out_fd);
      }
      if ((proc->out_fd = dup2(newfd, STDOUT_FILENO)) < 0) {
        goto redirect_error;
      }

      if (proc->err_fd >= 0) {
        close(proc->err_fd);
      }
      if ((proc->err_fd = dup2(newfd, STDERR_FILENO)) < 0) {
        goto redirect_error;
      }
      break;

    default:
      break;
  }

  return 0;

redirect_error:
  if (CleanupRedirection(proc) < 0) {
    PrintError("failed to cleanup redirection: %s\n", strerror(errno));
  }
  return -1;
}

/**
 * @brief Determines the type of redirection based on the operator.
 *
 * Analyzes the redirection operator provided as input and returns the
 * corresponding redirection type.
 *
 * @param op The redirection operator as a string (e.g., ">", ">>", "<").
 *
 * @return The `RedirectType` enumeration value corresponding to the operator,
 *         or kNone if the operator does not match any known redirection type.
 */
RedirectType GetRedirectType(const char *op) {
  if (strcmp(op, "<") == 0) {
    return kRedirectIn;
  }
  if (strcmp(op, ">") == 0 || strcmp(op, "1>") == 0) {
    return kRedirectOut;
  }
  if (strcmp(op, ">>") == 0) {
    return kRedirectAppend;
  }
  if (strcmp(op, "2>") == 0) {
    return kRedirectErr;
  }
  if (strcmp(op, "&>") == 0) {
    return kRedirectOutErr;
  }

  return kNone;
}

/**
 * @brief Cleans up redirections and restores original file descriptors.
 *
 * Closes any redirected file descriptors and restores the process's original
 * standard input, output, and error streams to their state before redirection.
 *
 * @param proc Pointer to the Process structure whose redirections are to be
 *             cleaned up.
 *
 * @return 0 on success, or -1 on error with errno set accordingly.
 */
int CleanupRedirection(Process *proc) {
  if (!proc) {
    return 0;
  }

  // Close redirected streams
  if (proc->in_fd >= 0) {
    close(proc->in_fd);
    proc->in_fd = -1;
  }
  if (proc->out_fd >= 0) {
    close(proc->out_fd);
    proc->out_fd = -1;
  }
  if (proc->err_fd >= 0) {
    close(proc->err_fd);
    proc->err_fd = -1;
  }

  // Restore original standard streams
  if (proc->orig_stdin >= 0) {
    if (dup2(proc->orig_stdin, STDIN_FILENO) < 0) {
      return -1;
    }
    close(proc->orig_stdin);
    proc->orig_stdin = -1;
  }

  if (proc->orig_stdout >= 0) {
    if (dup2(proc->orig_stdout, STDOUT_FILENO) < 0) {
      return -1;
    }
    close(proc->orig_stdout);
    proc->orig_stdout = -1;
  }

  if (proc->orig_stderr >= 0) {
    if (dup2(proc->orig_stderr, STDERR_FILENO) < 0) {
      return -1;
    }
    close(proc->orig_stderr);
    proc->orig_stderr = -1;
  }

  return 0;
}

/**
 * @brief Replaces the exit status variable in the command arguments.
 *
 * Searches for the exit status variable `$?` in the command arguments and
 * replaces it with the actual exit status of the last executed command.
 *
 * @param da_args Pointer to the DynamicArray containing the command arguments.
 * @param status  The exit status of the last executed command to substitute
 *                for `$?`.
 */
void ReplaceExitStatusVariable(DynamicArray *da_args, int status) {
  char **args = (char **)da_args->data;
  char status_str[12];

  sprintf(status_str, "%d", status);

  for (size_t i = 0; i < da_args->len; i++) {
    if (strcmp(args[i], "$?") == 0) {
      strcpy(args[i], status_str);
    }
  }
}

/**
 * @brief Signal handler for SIGINT.
 *
 * Prints a newline upon a SIGINT is received.
 *
 * @param signum The signal number of the received signal, unused in this
 *               handler.
 */
void sigint_handler(int signum __attribute__((unused))) { printf("\n"); }

/**
 * @brief Prints a formatted error message to stderr.
 *
 * A wrapper function that prints a formatted error message to standard error,
 * including the function name and line number from where the error originated,
 * improving debuggability.
 *
 * @param format The format string for the error message, followed by any
 *               arguments needed for formatting, similar to printf.
 */
void _PrintError(const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "shell: ");
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
  if (da->len >= da->size) {
    if (ResizeDynamicArray(da, da->size * 2) < 0) {
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
