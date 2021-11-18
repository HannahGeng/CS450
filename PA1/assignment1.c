#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), ; (sequence), '&' (parallel)
};

struct execcmd {
  int type;              // ' '
  int argc;
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

// Currently supports 32 commands
int cmd_index = 0;
struct execcmd g_exe_cmd[64];

FILE *log_fp;
#define __SHELL_DEBUG__ 1
#ifdef __SHELL_DEBUG__
#define SHELL_LOG(__format, __args...) do {\
        if (log_fp) {\
            fprintf(log_fp, __format, ##__args);\
            fflush(log_fp);\
        } else {\
            printf(__format, ##__args);\
        }\
} while (0)
#else
#define SHELL_LOG(__format, __args...)
#endif

#define PREFIX  "$CS450>"
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;";

int fork1(void) {
  int pid;

  pid = fork();
  if (pid == -1)
    perror("fork error:");

  return pid;
}

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char *mkcopy(char *s, char *es) {
  int n = es - s;

  char *c = malloc(n + 1);
  assert(c);

  strncpy(c, s, n);
  c[n] = 0;

  return c;
}

int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;

  if (q)
    *q = s;

  ret = *s;
  switch (*s) {
    case 0: break;

    case '|':
    case '<':
    case '>':
    case ';':
    case '&': s++;
      break;

    default: ret = 'a';
      while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
        s++;
      break;
  }

  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;

  *ps = s;

  return ret;
}

int peek(char **ps, char *es, char *toks) {
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;

  *ps = s;

  return *s && strchr(toks, *s);
}

struct cmd *parseexec(char **ps, char *es) {
  char *q, *eq;
  int tok;
  struct execcmd *cmd;
  int prev_spilte = 0;

  cmd = &g_exe_cmd[cmd_index++];

  while (!peek(ps, es, "|")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0) {
      SHELL_LOG("parse cmd end\n");
      break;
    }

    if (tok == ';' || tok == '&') {
      SHELL_LOG("get new cmd %d\n\n", cmd_index);
      cmd = &g_exe_cmd[cmd_index++];
      prev_spilte = 1;
      cmd->type = tok;
    } else {
      if (prev_spilte) {
        SHELL_LOG("prev_spilte, get new cmd %d\n\n", cmd_index);
        cmd = &g_exe_cmd[cmd_index++];
        prev_spilte = 0;
      }
    }
    cmd->argv[cmd->argc] = mkcopy(q, eq);
    SHELL_LOG("argv[%d]:%s\n", cmd->argc, cmd->argv[cmd->argc]);
    cmd->argc++;

    if (cmd->argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }

  }

  return (struct cmd *) &g_exe_cmd[0];
}

struct cmd *parseline(char **ps, char *es) {
  struct cmd *cmd;
  cmd = parseexec(ps, es);
  return cmd;
}

struct cmd *parsecmd(char *s) {
  char *es;
  struct cmd *cmd;
  int cmd_len = strlen(s);

  SHELL_LOG("cmd:%s,len %d\n", s, cmd_len);

  /**
   * 6) A command string terminated by a “&” is illegal; but it is fine with “;”’
   */
  if (s[cmd_len - 2] == '&') {
    fprintf(stderr, "the command string terminated by a “&” is illegal; but it is fine with “;”’\n");
    exit(-1);
  }

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");

  if (s != es) {
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

// Obtain input, need to ensure that buf is large enough, and the structure may be out of bounds
int getcmd(char *buf, int nbuf) {
  /**
   * 1) After your shell has started, it will give a prompt to the user. You should use
    “$CS450” for the prompt.
   */
  if (isatty(fileno(stdin))) {
    fprintf(stdout, PREFIX);
  }

  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if (buf[0] == 0) {// EOF
    return -1;
  }

  return 0;
}

/**
 * 5) Your shell shall support command lines with 3 or more commands connected by
 * the “;” and “&” operators. The operators are of equal rank and they will be
 * executed from left to right. As an example, the string “cmd1&cmd2;cmd3” will
 * see cmd1 and cmd2 executed in parallel, then Cmd3 after they both terminate.
*/
void run_do_cmds(int bindex, int eindex) {
  int i, j;
  int tmp_ecmd_index = 0;
  pid_t ecmd_pid[64];
  struct execcmd *g_tmp_ecmd[64];
  struct execcmd *ecmd;

  tmp_ecmd_index = 0;
  memset(g_tmp_ecmd, 0x00, sizeof(struct execcmd *) * 64);
  memset(ecmd_pid, 0x00, sizeof(pid_t) * 64);

  for (i = bindex; i < eindex; i++) {
    ecmd = &g_exe_cmd[i];

    SHELL_LOG("i %d cmd:", i);
    for (j = 0; j < ecmd->argc; j++) {
      SHELL_LOG("%s ", ecmd->argv[j]);
    }
    SHELL_LOG("\n");

    if (ecmd->type != '&') {
      g_tmp_ecmd[tmp_ecmd_index] = ecmd;
      tmp_ecmd_index++;
    }
  }

  // Execute specific commands
  SHELL_LOG("tmp_ecmd_index %d\n", tmp_ecmd_index);
  for (i = 0; i < tmp_ecmd_index; i++) {
    ecmd = g_tmp_ecmd[i];

    SHELL_LOG("execv cmd:");
    for (j = 0; j < ecmd->argc; j++) {
      SHELL_LOG("%s ", ecmd->argv[j]);
    }
    SHELL_LOG("\n");

    ecmd_pid[i] = fork1();
    if (ecmd_pid[i] == 0) {
      // The child process executes specific commands
      if (execv(ecmd->argv[0], ecmd->argv) == -1) {
        char mypath[20] = "/bin/";
        strcat(mypath, ecmd->argv[0]);
        if (execv(mypath, ecmd->argv) == -1) {
          strcpy(mypath, "/usr/bin/");
          strcat(mypath, ecmd->argv[0]);
          if (execv(mypath, ecmd->argv) == -1) {
            fprintf(stderr, "Command %s can't find\n", ecmd->argv[0]);
            _exit(0);
          }
        }
      }
    }
  }

  // The parent process waits for all child processes to return
  for (i = 0; i < tmp_ecmd_index; i++) {
    waitpid((pid_t) ecmd_pid[i], NULL, 0);
  }

}

void run_foreach_cmd(struct cmd *cmd) {
  int i;
  struct execcmd *ecmd;
  int bindex, eindex;

  if (cmd == NULL) {
    return;
  }

  SHELL_LOG("\nrun_foreach_cmd have %d cmds\n", cmd_index);
  if (cmd_index == 0) {
    return;
  }

  bindex = eindex = 0;
  while (1) {
    SHELL_LOG("------------------[%d-%d]-----------------\n", bindex, eindex);

    if (eindex == cmd_index) {
      SHELL_LOG("eindex %d == cmd_index %d, do cmds end, bindex %d\n", eindex,
                cmd_index, bindex);

      // Command traversal is complete, execute the remaining commands
      if (bindex < eindex) {
        run_do_cmds(bindex, eindex);
      }

      break;
    }

    ecmd = &g_exe_cmd[eindex];
    SHELL_LOG("bindex %d, eindex %d, ecmd argc %d, type %c\n", bindex, eindex, ecmd->argc,
              ecmd->type);

    SHELL_LOG("curr cmd:");
    for (i = 0; i < ecmd->argc; i++) {
      SHELL_LOG("%s ", ecmd->argv[i]);
    }
    SHELL_LOG("\n");

    if (ecmd->type == ';') { // sequence execute
      // First execute the command between bindex and eindex, and continue to parse the command
      SHELL_LOG("do [;] begin, bindex %d, eindex %d\n", bindex, eindex);
      run_do_cmds(bindex, eindex);

      eindex++;
      bindex = eindex;
      SHELL_LOG("do [;] end, eindex to %d, bindex to %d\n", eindex, bindex);
    } else if (ecmd->type == '&') {// parallel execute, need to continue to judge the need to parallel several commands
      eindex++;
      SHELL_LOG("do [&] bindex %d, eindex to %d\n", bindex, eindex);
    } else {// not; &, then you need to continue to judge whether there is any; &
      eindex++;
      SHELL_LOG("not [;&] bindex %d, eindex to %d\n", bindex, eindex);
    }
  }
}

int main(void) {
  static char buf[1024];
  int pid, status;

  log_fp = fopen("./shell_log", "w+");

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf) - 1] = 0;  // chop \n

      if (chdir(buf + 3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf + 3);

      continue;
    }

    cmd_index = 0;
    SHELL_LOG("get buf:%s\n", buf);
    if (buf[0] == '\n') {
      SHELL_LOG("buf have nothing\n");
      continue;
    }

    pid = fork1();
    if (pid == 0) {
      // Subprocess execution command parsing and execution
      run_foreach_cmd(parsecmd(buf));
      exit(0);
    }

    // The parent process waits for the end of the child process
    waitpid((pid_t) pid, &status, 0);
  }

  exit(0);
}

