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
  char cmdline[kInputMax];
  char *ps1 = getenv("PS1");

  while (1) {
    ps1 ? printf("%s", ps1) : ExpandPromptString();

    if (!fgets(cmdline, kInputMax, stdin)) {
      PrintError("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    cmdline[strlen(cmdline) - 1] = '\0';
  }

  return EXIT_SUCCESS;
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

void _PrintError(const char *func, int line, const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "[%s: %d] shell: ", func, line);
  vfprintf(stderr, format, args);
  
  va_end(args);
}
