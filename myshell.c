#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>



int prepare(void){
//  based on signal_info.c from recitation 3
//  ignore Ctrl+C
    struct sigaction newAction;
    memset(&newAction, 0, sizeof(newAction));
    newAction.sa_handler = SIG_IGN;
    newAction.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &newAction, NULL) == -1) {
        fprintf(stderr, "Signal handle registration failed, error: %s\n", strerror(errno));
        return 1;
    }
    //  Eran's trick
    signal(SIGCHLD, SIG_IGN);
    return 0;
}

int executing_commands(int count, char **arglist){
//    based on exec_fork.c from recitation 3:
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork() failed, error: %s\n", strerror(errno));
        exit(1);
    }
    if (pid == 0) {
        // Child process
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed, error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }
//  if we got here, this is the parent process

//  based on wait_demo.c from recitation 3:
    int exit_code = -1;
    pid_t child = -1;
    child = waitpid(pid, &exit_code, 0);
    if (-1 != (child = wait(&exit_code))) {
        fprintf(stderr, "wait failed, error: %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

int piping(int index, char **arglist) {
//   based on papa_son_pipe.c file from recitation 4
    int pipefd[2];
    pid_t cpid;

    if (-1 == pipe(pipefd)) {
        fprintf(stderr, "pipe failed, error: %s\n", strerror(errno));
        exit(1);
    }
    arglist[index] = NULL;
    cpid = fork();
    if (-1 == cpid) {
        fprintf(stderr, "fork() failed, error: %s\n", strerror(errno));
        exit(1);
    }
    if (0 == cpid) {
        if (dup2(pipefd[1], 1) == -1){
            fprintf(stderr, "dup2 failed, error: %s\n", strerror(errno));
            exit(1);
        }
        // Child reads from pipe
        // Close unused write and read end
        close(pipefd[0]);
        close(pipefd[1]);
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed, error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }
//  delete the '|' sign
    arglist[index] = NULL;
    pid_t cpid2 = fork();
    if (-1 == cpid2) {
        fprintf(stderr, "fork() failed, error: %s\n", strerror(errno));
        exit(1);
    }
    if (0 == cpid2) {
//      get the second command
        arglist = arglist + index + 1;
        close(pipefd[1]);
        if (dup2(pipefd[0], 0) == -1){
            fprintf(stderr, "dup2 failed, error: %s\n", strerror(errno));
            exit(1);
        }
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed, error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
        close(pipefd[0]);
        exit(0);
    }
    close(pipefd[0]);
    close(pipefd[1]);

    int exit_code = -1;
    pid_t child = -1;
    if (-1 != (child = wait(&exit_code))) {
        fprintf(stderr, "wait failed, error: %s\n", strerror(errno));
        exit(1);
    }

    int exit_code2 = -1;
    pid_t child2 = -1;
    if (-1 != (child2 = wait(&exit_code2))) {
        fprintf(stderr, "wait failed, error: %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

int Executing_in_background(int index, char **arglist) {
//    based on exec_fork.c from recitation 3:
//  delete the '&' sign
    arglist[index] = NULL;
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork() failed, error: %s\n", strerror(errno));
        exit(1);
    }
    if (pid == 0) {
        // Child process
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed, error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }
//  if we got here, this is the parent process, in this case we do nothing
//  because the parent doesn't wait for children
    return 0;
}
int Output_redirecting(int index, char **arglist) {
//   based on papa_son_pipe.c file from recitation 4
    pid_t cpid;
//  delete the '>' sign
    arglist[index] = NULL;

    cpid = fork();
    if (-1 == cpid) {
        fprintf(stderr, "fork() failed, error: %s\n", strerror(errno));
        exit(1);
    }
//  based on fifo_writer.c file from recitation 4
    int fd = open(arglist[index+1], O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "open failed, error: %s\n", strerror(errno));
        exit(1);
    }
    if (0 == cpid) {
        if (dup2(fd, 1) == -1){
            fprintf(stderr, "dup2 failed, error: %s\n", strerror(errno));
            exit(1);
        }
        // Child reads from pipe
        // Close unused write and read end
        close(fd);
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed, error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }

    int exit_code = -1;
    pid_t child = -1;
    if (-1 != (child = wait(&exit_code))) {
        fprintf(stderr, "wait failed, error: %s\n", strerror(errno));
        exit(1);
    }

    return 0;
}


int process_arglist(int count, char **arglist){
//  first part - check what is the wanted shell functionality
    int index = 0;
    int functionality = 0;
    for (int i = 0; i < count + 1; ++i) {
        if (strcmp(arglist[i],"&") == 0){
            functionality = 1;
            index = i;
            break;
        }
        if (strcmp(arglist[i],"|") == 0){
            functionality = 2;
            index = i;
            break;
        }
        if (strcmp(arglist[i],">") == 0){
            functionality = 3;
            index = i;
            break;
        }
    }
//  second part - handle each wanted fanctuality
    if (functionality == 0){
        executing_commands(count, arglist);
    }
    if (functionality == 1){
        Executing_in_background(index, arglist);
    }
    if (functionality == 2){
        piping(index, arglist);
    }
    if (functionality == 3){
        Output_redirecting(index, arglist);
    }
    return 1;

}

int finalize(void){
    return 0;
}