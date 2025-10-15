#include "../include/wsh.h"
#include "../include/dynamic_array.h"
#include "../include/utils.h"
#include "../include/hash_map.h"

#include <stdio.h>     // fprintf, fgets, fopen
#include <errno.h>     // errno
#include <stdarg.h>    // va_list, va_start
#include <stdlib.h>    // malloc, free, exit
#include <string.h>    // strcmp, strlen, strchr
#include <assert.h>    // assert
#include <unistd.h>    // fork, execv, access
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid, WIFEXITED
#include <limits.h>    // PATH_MAX
#include <signal.h>    // kill, SIGTERM

int rc; // return code 
HashMap *alias_hm = NULL; // hash map to store aliases 
DynamicArray *history_da = NULL; // dynamic array to command history
FILE *batch_file = NULL; // Global to track batch file for cleanup - memory leak fix


struct
{
  const char *name;
  int (*func)(int, char **);
} builtins[] = {
    {"exit", wsh_exit},
    {"alias", wsh_alias},
    {"unalias", wsh_unalias},
    {"which", wsh_which},
    {"path", wsh_path},
    {"cd", wsh_cd},
    {"history", wsh_history},
    {NULL, NULL}};

/**
 *Terminates the shell
*/
int wsh_exit(int argc, char **argv)
{
  assert(strcmp(*argv, "exit") == 0);
  if (argc > 1)
  {
    wsh_warn(INVALID_EXIT_USE);
    return EXIT_FAILURE;
  }
  clean_exit(rc);
  return EXIT_SUCCESS;
}

/**
 * Displays previously executed commands
*/
int wsh_history(int argc, char **argv)
{
  if (argc > 2)
  {
    wsh_warn(INVALID_HISTORY_USE);
    return EXIT_FAILURE;
  }

  if (argc == 1)
  {
    da_print(history_da);
    fflush(stdout);
    return EXIT_SUCCESS;
  }
  else
  {
    const char *arg = argv[1];
    long n;
    char *endptr;

    n = strtol(arg, &endptr, 10);

    if (endptr == arg || *endptr != '\0' || n <= 0)
    {
      wsh_warn(HISTORY_INVALID_ARG);
      return EXIT_FAILURE;
    }

    if ((size_t)n > history_da->size)
    {
      wsh_warn(HISTORY_INVALID_ARG);
      return EXIT_FAILURE;
    }

    fprintf(stdout, "%s", history_da->data[n - 1]);
    fflush(stdout);
    return EXIT_SUCCESS;
  }
}

/**
 * Manages command aliases
*/
int wsh_alias(int argc, char **argv)
{
  if (argc == 1)
  {
    hm_print_sorted(alias_hm);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  if (argc > 4 || strcmp(argv[2], "=") != 0)
  {
    wsh_warn(INVALID_ALIAS_USE);
    return EXIT_FAILURE;
  }

  const char *name = argv[1];
  const char *command = " ";
  if (argc == 4)
  {
    command = argv[3];
  }

  if (strlen(name) == 0)
  {
    wsh_warn(INVALID_ALIAS_USE);
    return EXIT_FAILURE;
  }

  hm_put(alias_hm, name, command);
  return EXIT_SUCCESS;
}

/**
 * Removes an existing alias
*/
int wsh_unalias(int argc, char **argv)
{
  if (argc != 2)
  {
    wsh_warn(INVALID_UNALIAS_USE);
    return EXIT_FAILURE;
  }

  hm_delete(alias_hm, argv[1]);
  return EXIT_SUCCESS;
}

/**
 * Searches for an executable command
 */
int wsh_which(int argc, char **argv)
{
  if (argc != 2)
  {
    wsh_warn(INVALID_WHICH_USE);
    return EXIT_FAILURE;
  }

  const char *name = argv[1];

  char *alias_cmd = hm_get(alias_hm, name);
  if (alias_cmd)
  {
    fprintf(stdout, WHICH_ALIAS, name, alias_cmd);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  for (int i = 0; builtins[i].name != NULL; i++)
  {
    if (strcmp(name, builtins[i].name) == 0)
    {
      fprintf(stdout, WHICH_BUILTIN, name);
      fflush(stdout);
      return EXIT_SUCCESS;
    }
  }

  char *full_path = NULL;

  if (name[0] == '.' || name[0] == '/')
  {
    if (access(name, X_OK) == 0)
    {
      full_path = strdup(name);
      if (!full_path)
      {
        perror("strdup");
        clean_exit(EXIT_FAILURE);
      }
    }
  }
  else
  {
    char *path_env = getenv("PATH");

    if (path_env != NULL && strlen(path_env) > 0)
    {
      char *path_copy = strdup(path_env);
      if (!path_copy)
      {
        perror("strdup");
        clean_exit(EXIT_FAILURE);
      }

      char *dir = strtok(path_copy, ":");
      while (dir != NULL)
      {
        size_t path_len = strlen(dir) + strlen(name) + 2;
        char *temp_path = malloc(path_len);
        if (!temp_path)
        {
          perror("malloc");
          free(path_copy);
          clean_exit(EXIT_FAILURE);
        }

        sprintf(temp_path, "%s/%s", dir, name);

        if (access(temp_path, X_OK) == 0)
        {
          full_path = temp_path;
          break;
        }

        free(temp_path);
        dir = strtok(NULL, ":");
      }
      free(path_copy);
    }
  }

  if (full_path)
  {
    fprintf(stdout, WHICH_EXTERNAL, name, full_path);
    fflush(stdout);
    free(full_path);
    return EXIT_SUCCESS;
  }
  else
  {
    fprintf(stdout, WHICH_NOT_FOUND, name);
    fflush(stdout);
    return EXIT_FAILURE;
  }
}

/**
 * Views and modifies the PATH environment variable
 */
int wsh_path(int argc, char **argv)
{
  if (argc > 2)
  {
    wsh_warn(INVALID_PATH_USE);
    return EXIT_FAILURE;
  }

  if (argc == 1)
  {
    char *path_env = getenv("PATH");
    if (path_env)
    {
      fprintf(stdout, "%s\n", path_env);
      fflush(stdout);
    }
    return EXIT_SUCCESS;
  }
  else
  {
    if (setenv("PATH", argv[1], 1) == -1)
    {
      perror("setenv");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
}

/**
 * Changes the current working directory
 */
int wsh_cd(int argc, char **argv)
{
  if (argc > 2)
  {
    wsh_warn(INVALID_CD_USE);
    return EXIT_FAILURE;
  }

  char *dir_to_go = NULL;

  if (argc == 1)
  {
    dir_to_go = getenv("HOME");
    if (!dir_to_go)
    {
      wsh_warn(CD_NO_HOME);
      return EXIT_FAILURE;
    }
  }
  else
  {
    dir_to_go = argv[1];
  }

  if (chdir(dir_to_go) == -1)
  {
    perror("cd");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/***************************************************
 * Helper Functions
 ***************************************************/
/**
 * @Brief Free any allocated global resources
 */
void wsh_free(void)
{
   // Free any allocated resources here
  if (alias_hm != NULL)
  {
    hm_free(alias_hm);
    alias_hm = NULL;
  }
  if (history_da != NULL)
  {
    da_free(history_da);
    history_da = NULL;
  }
  if (batch_file != NULL)
  {
    fclose(batch_file);
    batch_file = NULL;
  }
}

/**
 * @Brief Cleanly exit the shell after freeing resources
 *
 * @param return_code The exit code to return
 */
void clean_exit(int return_code)
{
  wsh_free();
  exit(return_code);
}

/**
 * @Brief Print a warning message to stderr and set the return code
 *
 * @param msg The warning message format string
 * @param ... Additional arguments for the format string
 */
void wsh_warn(const char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  fflush(stderr);
  va_end(args);
  rc = EXIT_FAILURE;
}

/**
 * Frees argument array
 */
void free_argv(char **argv, int argc)
{
  for (int i = 0; i < argc; i++)
  {
    free(argv[i]);
    argv[i] = NULL;
  }
}

/**
 * Executes an external command by forking and using execv
 */
int execute_external_command(int argc, char **argv)
{
  assert(argc > 0);
  char *command_name = argv[0];
  char *full_path = NULL;
  int is_absolute_or_relative = 0;

  if (command_name[0] == '/' || (command_name[0] == '.' && command_name[1] == '/'))
  {
    is_absolute_or_relative = 1;
    full_path = strdup(command_name);
    if (!full_path)
    {
      perror("strdup");
      clean_exit(EXIT_FAILURE);
    }
  }
  else
  {
    full_path = find_executable_path(command_name);
    if (!full_path)
    {
      char *path_env = getenv("PATH");
      if (path_env == NULL || strlen(path_env) == 0)
      {
        wsh_warn(EMPTY_PATH);
        return EXIT_FAILURE;
      }
    }
  }

  if (!full_path || (is_absolute_or_relative && access(full_path, X_OK) != 0))
  {
    if (full_path)
      free(full_path);
    wsh_warn(CMD_NOT_FOUND, command_name);
    return EXIT_FAILURE;
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    perror("fork");
    free(full_path);
    return EXIT_FAILURE;
  }
  else if (pid == 0)
  {
    execv(full_path, argv);
    wsh_warn(CMD_NOT_FOUND, command_name);
    free(full_path);
    _exit(EXIT_FAILURE);
  }
  else
  {
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
      perror("waitpid");
      free(full_path);
      return EXIT_FAILURE;
    }

    free(full_path);
    if (WIFEXITED(status))
    {
      rc = WEXITSTATUS(status);
    }
    else
    {
      rc = EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
}

/**
 * Executes a single command line with alias substitution
 */
int execute_command(const char *cmdline)
{
  if (cmdline != NULL && strlen(cmdline) > 0)
  {
    char *temp = strdup(cmdline);
    if (temp)
    {
      size_t len = strlen(temp);
      if (len > 0 && temp[len - 1] == '\n')
        temp[len - 1] = '\0';
      if (strspn(temp, " \t") != strlen(temp))
      {
        da_put(history_da, cmdline);
      }
      free(temp);
    }
  }

  char *cmdline_copy = strdup(cmdline);
  if (!cmdline_copy)
  {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }

  char *segments[MAX_ARGS];
  int num_segments = 0;
  int is_pipeline = 0;
  int result = EXIT_SUCCESS;

  if (strchr(cmdline, '|'))
  {
    is_pipeline = 1;
  }

  if (is_pipeline)
  {
    char *p = cmdline_copy;
    char *segment_start = p;

    while ((p = strchr(segment_start, '|')) != NULL && num_segments < MAX_ARGS)
    {
      *p = '\0';

      char *s = segment_start;
      while (*s && *s == ' ')
        s++;
      if (*s == '\0')
      {
        wsh_warn(EMPTY_PIPE_SEGMENT);
        for (int j = 0; j < num_segments; j++)
          free(segments[j]);
        free(cmdline_copy);
        return EXIT_FAILURE;
      }

      segments[num_segments] = strdup(segment_start);
      if (!segments[num_segments])
      {
        perror("strdup");
        free(cmdline_copy);
        clean_exit(EXIT_FAILURE);
      }
      num_segments++;
      segment_start = p + 1;
    }

    if (num_segments < MAX_ARGS)
    {
      char *s = segment_start;
      while (*s && *s == ' ')
        s++;
      if (*s == '\0' || *s == '\n')
      {
        wsh_warn(EMPTY_PIPE_SEGMENT);
        for (int j = 0; j < num_segments; j++)
          free(segments[j]);
        free(cmdline_copy);
        return EXIT_FAILURE;
      }

      segments[num_segments] = strdup(segment_start);
      if (!segments[num_segments])
      {
        perror("strdup");
        free(cmdline_copy);
        clean_exit(EXIT_FAILURE);
      }
      num_segments++;
    }
    free(cmdline_copy);

    for (int i = 0; i < num_segments; i++)
    {
      char *temp_argv[MAX_ARGS + 1];
      int temp_argc;
      char *segment_cmdline = segments[i];
      char *temp_alias_subst = NULL;

      parseline_no_subst(segment_cmdline, temp_argv, &temp_argc);

      if (temp_argc == 0)
      {
        wsh_warn(EMPTY_PIPE_SEGMENT);
        for (int j = 0; j < num_segments; j++)
          free(segments[j]);
        free_argv(temp_argv, temp_argc);
        return EXIT_FAILURE;
      }

      char *command_name = temp_argv[0];
      char *alias_cmd = hm_get(alias_hm, command_name);

      if (alias_cmd)
      {
        size_t new_cmdline_len = strlen(alias_cmd);
        for (int k = 1; k < temp_argc; k++)
        {
          new_cmdline_len += 1 + strlen(temp_argv[k]);
        }
        new_cmdline_len += 1;

        temp_alias_subst = malloc(new_cmdline_len);
        if (!temp_alias_subst)
        {
          perror("malloc");
          for (int j = 0; j < num_segments; j++)
            free(segments[j]);
          free_argv(temp_argv, temp_argc);
          clean_exit(EXIT_FAILURE);
        }

        strcpy(temp_alias_subst, alias_cmd);
        for (int k = 1; k < temp_argc; k++)
        {
          strcat(temp_alias_subst, " ");
          strcat(temp_alias_subst, temp_argv[k]);
        }

        free_argv(temp_argv, temp_argc);
        parseline_no_subst(temp_alias_subst, temp_argv, &temp_argc);
        command_name = temp_argv[0];

        free(segments[i]);
        segments[i] = temp_alias_subst;
      }

      int is_builtin = 0;
      for (int j = 0; builtins[j].name != NULL; j++)
      {
        if (strcmp(command_name, builtins[j].name) == 0)
        {
          is_builtin = 1;
          break;
        }
      }

      if (!is_builtin)
      {
        char *path_to_exec = find_executable_path(command_name);
        if (path_to_exec == NULL)
        {
          wsh_warn(CMD_NOT_FOUND, command_name);
          for (int j = 0; j < num_segments; j++)
            free(segments[j]);
          free_argv(temp_argv, temp_argc);
          return EXIT_FAILURE;
        }
        free(path_to_exec);
      }

      free_argv(temp_argv, temp_argc);
    }

    result = execute_pipeline(segments, num_segments);

    for (int i = 0; i < num_segments; i++)
      free(segments[i]);
  }
  else
  {
    char *argv[MAX_ARGS + 1];
    int argc = 0;
    char *temp_cmdline = NULL;

    parseline_no_subst(cmdline, argv, &argc);

    if (argc == 0)
    {
      free(cmdline_copy);
      return EXIT_SUCCESS;
    }

    char *command = argv[0];
    char *alias_cmd = hm_get(alias_hm, command);

    if (alias_cmd)
    {
      size_t new_cmdline_len = strlen(alias_cmd);
      for (int k = 1; k < argc; k++)
      {
        new_cmdline_len += 1 + strlen(argv[k]);
      }
      new_cmdline_len += 1;

      temp_cmdline = malloc(new_cmdline_len);
      if (!temp_cmdline)
      {
        perror("malloc");
        free_argv(argv, argc);
        free(cmdline_copy);
        clean_exit(EXIT_FAILURE);
      }

      strcpy(temp_cmdline, alias_cmd);
      for (int k = 1; k < argc; k++)
      {
        strcat(temp_cmdline, " ");
        strcat(temp_cmdline, argv[k]);
      }

      free_argv(argv, argc);
      parseline_no_subst(temp_cmdline, argv, &argc);
      command = argv[0];

      if (argc == 0)
      {
        result = EXIT_SUCCESS;
        free(temp_cmdline);
        free(cmdline_copy);
        return result;
      }
    }

    // Handle exit separately to ensure proper memory cleanup
    if (strcmp(command, "exit") == 0)
    {
      if (argc > 1)
      {
        wsh_warn(INVALID_EXIT_USE);
        result = EXIT_FAILURE;
      }
      else
      {
        // Free all allocated memory before exiting
        free_argv(argv, argc);
        if (temp_cmdline)
          free(temp_cmdline);
        free(cmdline_copy);
        clean_exit(rc);
      }
    }
    else
    {
      // Check for other builtins
      int is_builtin = 0;
      for (int i = 0; builtins[i].name != NULL; i++)
      {
        if (strcmp(command, builtins[i].name) == 0)
        {
          is_builtin = 1;
          rc = builtins[i].func(argc, argv);
          result = rc == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
          break;
        }
      }

      if (!is_builtin)
      {
        result = execute_external_command(argc, argv);
      }
    }

    free_argv(argv, argc);
    if (temp_cmdline)
      free(temp_cmdline);
    free(cmdline_copy);
  }

  return result;
}

/**
 * Finds the full path to an executable command
 */
char *find_executable_path(const char *command_name)
{
  if (command_name[0] == '/' || (command_name[0] == '.' && command_name[1] == '/'))
  {
    if (access(command_name, X_OK) == 0)
    {
      return strdup(command_name);
    }
    return NULL;
  }

  char *path_env = getenv("PATH");
  if (path_env == NULL || strlen(path_env) == 0)
  {
    return NULL;
  }

  char *path_copy = strdup(path_env);
  if (!path_copy)
  {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }

  char *dir = strtok(path_copy, ":");
  char *full_path = NULL;
  while (dir != NULL)
  {
    size_t path_len = strlen(dir) + strlen(command_name) + 2;
    char *temp_path = malloc(path_len);
    if (!temp_path)
    {
      perror("malloc");
      free(path_copy);
      clean_exit(EXIT_FAILURE);
    }

    sprintf(temp_path, "%s/%s", dir, command_name);
    if (access(temp_path, X_OK) == 0)
    {
      full_path = temp_path;
      break;
    }
    free(temp_path);
    dir = strtok(NULL, ":");
  }
  free(path_copy);
  return full_path;
}

/**
 * Executes a single command segment in a pipeline
 */
void execute_segment(const char *segment_cmdline, int in_fd, int out_fd)
{
  char *argv[MAX_ARGS + 1];
  int argc;

  parseline_no_subst(segment_cmdline, argv, &argc);

  if (argc == 0)
  {
    free_argv(argv, argc);
    _exit(EXIT_FAILURE);
  }

  if (in_fd != STDIN_FILENO)
  {
    if (dup2(in_fd, STDIN_FILENO) == -1)
    {
      perror("dup2 (in_fd)");
      free_argv(argv, argc);
      _exit(EXIT_FAILURE);
    }
    close(in_fd);
  }

  if (out_fd != STDOUT_FILENO)
  {
    if (dup2(out_fd, STDOUT_FILENO) == -1)
    {
      perror("dup2 (out_fd)");
      free_argv(argv, argc);
      _exit(EXIT_FAILURE);
    }
    close(out_fd);
  }

  char *command_name = argv[0];

  for (int i = 0; builtins[i].name != NULL; i++)
  {
    if (strcmp(command_name, builtins[i].name) == 0)
    {
      int exit_code = builtins[i].func(argc, argv);
      free_argv(argv, argc);
      _exit(exit_code);
    }
  }

  char *path_to_exec = find_executable_path(command_name);

  if (!path_to_exec)
  {
    wsh_warn(CMD_NOT_FOUND, command_name);
    free_argv(argv, argc);
    _exit(EXIT_FAILURE);
  }

  execv(path_to_exec, argv);

  perror("execv");
  free(path_to_exec);
  free_argv(argv, argc);
  _exit(EXIT_FAILURE);
}

/**
 * Executes a pipeline of commands concurrently
 */
int execute_pipeline(char **segments, int num_segments)
{
  int i;
  int prev_pipe_read_fd = STDIN_FILENO;
  int pids[MAX_ARGS];

  for (i = 0; i < num_segments - 1; i++)
  {
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
      perror("pipe");
      for (int j = 0; j < i; j++)
        kill(pids[j], SIGTERM);
      return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
      perror("fork");
      close(pipefd[0]);
      close(pipefd[1]);
      for (int j = 0; j < i; j++)
        kill(pids[j], SIGTERM);
      return EXIT_FAILURE;
    }
    else if (pid == 0)
    {
      close(pipefd[0]);
      execute_segment(segments[i], prev_pipe_read_fd, pipefd[1]);
    }
    else
    {
      pids[i] = pid;
      close(pipefd[1]);

      if (prev_pipe_read_fd != STDIN_FILENO)
      {
        close(prev_pipe_read_fd);
      }

      prev_pipe_read_fd = pipefd[0];
    }
  }

  pid_t pid_last = fork();
  if (pid_last < 0)
  {
    perror("fork");
    for (int j = 0; j < i; j++)
      kill(pids[j], SIGTERM);
    return EXIT_FAILURE;
  }
  else if (pid_last == 0)
  {
    execute_segment(segments[i], prev_pipe_read_fd, STDOUT_FILENO);
  }
  else
  {
    pids[i] = pid_last;
    if (prev_pipe_read_fd != STDIN_FILENO)
    {
      close(prev_pipe_read_fd);
    }
  }

  int status;
  int all_success = EXIT_SUCCESS;

  if (waitpid(pid_last, &status, 0) == -1)
  {
    perror("waitpid");
    rc = EXIT_FAILURE;
    all_success = EXIT_FAILURE;
  }
  else if (WIFEXITED(status))
  {
    rc = WEXITSTATUS(status);
    if (rc != EXIT_SUCCESS)
      all_success = EXIT_FAILURE;
  }
  else
  {
    rc = EXIT_FAILURE;
    all_success = EXIT_FAILURE;
  }

  for (int j = 0; j < num_segments - 1; j++)
  {
    if (waitpid(pids[j], &status, 0) == -1)
    {
      perror("waitpid");
      all_success = EXIT_FAILURE;
    }
  }

  return all_success;
}

/**
 * @Brief Main entry point for the shell
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return
 */
int main(int argc, char **argv)
{
  alias_hm = hm_create();
  history_da = da_create(0);
  setenv("PATH", "/bin:/usr/bin", 1);

  if (argc > 2)
  {
    wsh_warn(INVALID_WSH_USE);
    return EXIT_FAILURE;
  }

  switch (argc)
  {
  case 1:
    interactive_main();
    break;
  case 2:
    rc = batch_main(argv[1]);
    break;
  default:
    break;
  }

  wsh_free();
  return rc;
}

/***************************************************
 * Modes of Execution
 ***************************************************/

/**
 * @Brief Interactive mode: print prompt and wait for user input
 * execute the given input and repeat
 */
void interactive_main(void)
{
  char cmdline[MAX_LINE];
  while (1)
  {
    fprintf(stdout, PROMPT);
    fflush(stdout);

    if (!fgets(cmdline, sizeof(cmdline), stdin))
    {
      if (ferror(stdin))
      {
        fprintf(stderr, "fgets error\n");
        fflush(stderr);
        clearerr(stdin);
      }
      clean_exit(rc);
    }

    execute_command(cmdline);
  }
  clean_exit(rc);
}

/**
 * @Brief Batch mode: read commands from script file line by line
 * execute each command and repeat until EOF
 *
 * @param script_file Path to the script file
 * @return EXIT_SUCCESS(0) on success, EXIT_FAILURE(1) on error
 */
int batch_main(const char *script_file)
{
  FILE *fp = fopen(script_file, "r");
  if (fp == NULL)
  {
    perror("fopen");
    rc = EXIT_FAILURE;
    return EXIT_FAILURE;
  }

  batch_file = fp; // Store globally for cleanup in wsh_free

  char cmdline[MAX_LINE];
  int result = EXIT_SUCCESS;

  while (fgets(cmdline, MAX_LINE, fp) != NULL)
  {
    result = execute_command(cmdline);
  }

  fclose(fp);
  batch_file = NULL; // Clear after closing
  return result;
}

/***************************************************
 * Parsing
 ***************************************************/

/**
 * @Brief Parse a command line into arguments without doing
 * any alias substitutions.
 * Handles single quotes to allow spaces within arguments.
 *
 * @param cmdline The command line to parse
 * @param argv Array to store the parsed arguments (must be preallocated)
 * @param argc Pointer to store the number of parsed arguments
 */
void parseline_no_subst(const char *cmdline, char **argv, int *argc)
{
  if (!cmdline)
  {
    *argc = 0;
    argv[0] = NULL;
    return;
  }
  char *buf = strdup(cmdline);
  if (!buf)
  {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }
  /* Replace trailing newline with space */
  const size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = ' ';
  else
  {
    char *new_buf = realloc(buf, len + 2);
    if (!new_buf)
    {
      perror("realloc");
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    buf = new_buf;
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;
  while (*p && *p == ' ')
    p++; /* skip leading spaces */

  while (*p)
  {
    char *token_start = p;
    char *token = NULL;
    if (*p == '\'')
    {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token)
      {
        /* Handle missing closing quote - Print Missing closing quote to stderr */
        wsh_warn(MISSING_CLOSING_QUOTE);
        free(buf);
        for (int i = 0; i < count; i++)
          free(argv[i]);
        *argc = 0;
        argv[0] = NULL;
        return;
      }
      *token = '\0';
      p = token + 1;
    }
    else
    {
      token = strchr(p, ' ');
      if (!token)
        break;
      *token = '\0';
      p = token + 1;
    }
    argv[count] = strdup(token_start);
    if (!argv[count])
    {
      perror("strdup");
      for (int i = 0; i < count; i++)
        free(argv[i]);
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    count++;
    while (*p && (*p == ' '))
      p++;
  }
  argv[count] = NULL;
  *argc = count;
  free(buf);
}
