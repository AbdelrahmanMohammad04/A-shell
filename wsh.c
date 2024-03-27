#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>

int history_max_size = 1000;
char *history[1000];
int history_count = 1;
int size = 5;

char* vars[100];
char* vals[100];
int vidx = 0;


int wsh_exit(char **args)
{
  return 0;
}

int wsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "wsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("wsh");
    }
  }
  return 1;
}


int wsh_history(char **args)
{
  int tempsize = size;
  
  if (args[3]) {
    printf("history: too many arguments\n");
    return 1;
  } else if (args[1]) {
    if (strcmp(args[1], "set") == 0) {
      size = atoi(args[2]);
      for (int i = history_max_size - history_count + size + 1; i < history_max_size; i++) {
        history[i] = NULL;
      }
      return 1;
    }
    if (atoi(args[1]) == 0) {
        printf("history: %s: numeric argument required\n", args[1]);
        return 1;
    }
    tempsize = atoi(args[1]);
  }

  int index = 1;
  for (int i = history_max_size - history_count; i < history_max_size - history_count + tempsize + 1; i++) {
    if (i > history_max_size) {
      break;
    }
    if (history[i] != NULL) {
      printf("%i) %s", index, history[i]);
      index++;
    }
  }
  return 1;
}

int wsh_localvars(char **args)
{
  int position = 0;
  char **tokens = malloc(128 * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "wsh: allocation error\n");
    exit(1);
  }

  token = strtok(args[1], "=");
  while (token != NULL) {
    tokens[position] = token;
    position++;
    token = strtok(NULL, "=");
  }
  tokens[position] = NULL;
  if (position < 2) {
    for (int i = 0; i < 1; i++) {
      for (int j = 0; j < vidx; j++) {
        if (vars[j] == NULL) {
          continue;
        }
        if (strcmp(tokens[i], vars[j]) == 0) {
          vals[j] = NULL;
          vars[j] = NULL;
          return 1;
        }
      }
      vidx--;
    }
    return 1;
  }
  for (int i = 0; i < 1; i++) {
    for (int j = 0; j < vidx; j++) {
      if (vars[j] == NULL) {
        continue;
      }
      if (strcmp(tokens[i], vars[j]) == 0) {
        vals[j] = strdup(tokens[i+1]);
        return 1;
      }
    }
    vars[vidx] = strdup(tokens[i]);
    vals[vidx] = strdup(tokens[i+1]);
    vidx++;
  }

  return 1;
}

int wsh_printvars(char **args) {
  for (int i = 0; i < vidx; i++) {
    if (vars[i] == NULL) continue;
    printf("%s=%s\n", vars[i], vals[i]);
  }
  return 1;
}

int wsh_export(char **args) {
  char *name = strtok(args[1], "=");
  char *value = strtok(NULL, "");

  if (value == NULL) {
    unsetenv(name);
    return 1;
  }
  if (name != NULL && value != NULL) {
    setenv(name, value, 1);
  }
  return 1;
}

char *builtin_str[] = {
  "exit",
  "cd",
  "history",
  "local",
  "vars",
  "export"
};

int (*builtin_func[]) (char **) = {
  &wsh_exit,
  &wsh_cd,
  &wsh_history,
  &wsh_localvars,
  &wsh_printvars,
  &wsh_export
};

char *read_line(void)
{
  char *line = NULL;
  size_t bufsize = 0;

  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
        // printf("\n");
        exit(0);
    } else {
        perror("readline");
        exit(1);
    }
  }

  return line;
}


char* var_sub(char* var) {
  if (getenv(var)) {
    return getenv(var);
  }

  for (int i = 0; i < vidx; i++) {
    if (vars[i] == NULL) {
      continue;
    }
    if (strcmp(vars[i], var) == 0) {
      return vals[i];
    }
  }

  return NULL;
}

char** split_line(char* line) {
  int bufsize = 128, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "wsh: allocation error\n");
    exit(1);
  }

  token = strtok(line, " \t\r\n\a");
  while (token != NULL) {
    if (token[0] == '$') {
        char* var_name = token + 1;
        char* var_value = var_sub(var_name);
        if (var_value && strlen(var_value) > 0) {
          tokens[position++] = strdup(var_value);
        } 
    } else { 
        tokens[position++] = token;
    }

    token = strtok(NULL, " \t\r\n\a");
  }
  tokens[position] = NULL;
  return tokens;
}

int launch(char **args)
{
  pid_t pid, wpid;
  int status;

  if (1 == 0) {
    printf("%i\n",wpid);
  }

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
        perror("execvp");
    }
    exit(1);
  } else if (pid < 0) {
    // Error forking
    perror("wsh");
  } else {
    // Parent process
    do {
        wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

void add_command_to_history(char *command)
{
  if (history[history_max_size - history_count + 1] && strcmp(history[history_max_size - history_count + 1], command) == 0) {
    return;
  }
  history[history_max_size-history_count] = strdup(command);
  history_count = history_count + 1;
}

int parse_pipe(char **args, char ***command1, char ***command2) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            *command1 = args;
            *command2 = &args[i + 1];
            args[i] = NULL;
            return 1;
        }
        i++;
    }
    return 0;
}

int execute_pipe(char **args) {
    int pipefd[2];
    pid_t pid1, pid2;

    char **command1;
    char **command2;

    if (parse_pipe(args, &command1, &command2)) {
        if (pipe(pipefd) == -1) {
            perror("wsh: pipe");
            return 1;
        }
        pid1 = fork();
        if (pid1 == -1) {
            perror("wsh: fork");
            return 1;
        }
        if (pid1 == 0) {
            // Child process 1
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Send stdout to the pipe
            close(pipefd[1]); // Close write end after dup2

            if (execvp(command1[0], command1) == -1) {
                perror("wsh");
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process
            pid2 = fork();
            if (pid2 == -1) {
                perror("wsh: fork");
                return 1;
            }
            if (pid2 == 0) {
                // Child process 2
                close(pipefd[1]); // Close unused write end
                dup2(pipefd[0], STDIN_FILENO); // Receive stdin from the pipe
                close(pipefd[0]); // Close read end after dup2

                if (execvp(command2[0], command2) == -1) {
                    perror("wsh");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Parent process
                close(pipefd[0]);
                close(pipefd[1]);
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
        }
        return 1;
    }
    return 0; // No pipe to execute
}

int execute(char **args, char* line) {
    if (args[0] == NULL) {
        return 1;
    }

    int size = sizeof(builtin_str) / sizeof(builtin_str[0]);
    for (int i = 0; i < size; i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    if (execute_pipe(args)) {
        return 1; // Command executed via pipe
    }

    add_command_to_history(line);
    return launch(args);
}

int loop(int batch, int argc, char *argv[]) {
    char *line;
    char **args;
    int status;

    do {
        printf("wsh> ");
        line = read_line();
        char line2[128];
        strcpy(line2, line);
        args = split_line(line);
        status = execute(args, line2);

        free(line);
        free(args);
    } while (status);

    return 0;
}

int batch(int argc, char batchPath[]) {
    FILE* batchPtr = fopen(batchPath, "r");
    char batchContents[2048] = "";
    if (batchPtr == NULL) {
        printf("ERROR: Can't open batch file\n");
        return 1;
    } else {
        if (fgetc(batchPtr) == EOF) {
            printf("ERROR: Batch File Empty\n");
            fclose(batchPtr);
            return 1;
        }
        fseek(batchPtr, 0, SEEK_SET);

        while (fgets(batchContents, 512, batchPtr) != NULL) {
            char* batchArgv[128];
            int batchArgc = 1;

            char* tok = strtok(batchContents, " \n");
            while (tok != NULL) {
                batchArgv[batchArgc] = tok;
                batchArgc++;
                tok = strtok(NULL, " \n");
            }
            loop(1, batchArgc, batchArgv);
        }
        fclose(batchPtr);
    }
    return 0;
}

int main(int argc, char *argv[]) {    
    signal(SIGINT, SIG_IGN);
    if (argc > 2) {
        printf("USAGE:\n\t./wsh\n\t\tOR\n\t./wsh <batch file>\n");
        return 1;
    }

    int batchMode = argc == 2;

    if (batchMode) {
        batch(argc, argv[1]);
    } else {
        loop(0, argc, argv);
    }
    return 0;
}