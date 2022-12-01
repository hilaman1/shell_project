#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
        fprintf(stderr, "Signal handle registration failed on error: %s\n", strerror(errno));
        return 1;
    }
    //  Eran's trick
    signal(SIGCHLD, SIG_IGN);
    return 0;
}

void default_child_handler(){
    struct sigaction childHandler;
    memset(&childHandler, 0, sizeof(childHandler));
    childHandler.sa_handler = SIG_DFL;
    childHandler.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &childHandler, NULL) == -1) {
        fprintf(stderr, "child handler failed on error: %s\n", strerror(errno));
        exit(1);
    }
}


int executing_commands(char **arglist){
//    based on exec_fork.c from recitation 3:
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork failed on error: %s\n", strerror(errno));
        return 0;
    }
    if (pid == 0) {
        // Child process
        default_child_handler();
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed on error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }
//  if we got here, this is the parent process

//  based on wait_demo.c from recitation 3:
    int exit_code = -1;
    int child = waitpid(pid, &exit_code, 0);
    if ((-1 == child) && (errno != ECHILD)  && (errno != EINTR )) {
        fprintf(stderr, "wait failed on error: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int piping(int index, char **arglist) {
//  delete the '|' sign
    arglist[index] = NULL;
//   based on papa_son_pipe.c file from recitation 4
    int pipefd[2];
    pid_t cpid;

    if (-1 == pipe(pipefd)) {
        fprintf(stderr, "pipe failed on error: %s\n", strerror(errno));
        return 0;
    }
    arglist[index] = NULL;
    cpid = fork();
    if (-1 == cpid) {
        fprintf(stderr, "fork failed on error: %s\n", strerror(errno));
        return 0;
    }
    if (0 == cpid) {
        default_child_handler();
        if (dup2(pipefd[1], 1) == -1){
            fprintf(stderr, "dup2 failed on error: %s\n", strerror(errno));
            exit(1);
        }
        // Close unused write and read end
        close(pipefd[0]);
        close(pipefd[1]);
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed on error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }

    pid_t cpid2 = fork();
    if (-1 == cpid2) {
        fprintf(stderr, "fork failed on error: %s\n", strerror(errno));
        return 0;
    }
    if (0 == cpid2) {
        default_child_handler();
//      get the second command
        arglist = arglist + index + 1;
        close(pipefd[1]);
        if (dup2(pipefd[0], 0) == -1){
            fprintf(stderr, "dup2 failed on error: %s\n", strerror(errno));
            exit(1);
        }
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed on error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
        close(pipefd[0]);
        exit(0);
    }
// Close unused write and read end
    close(pipefd[0]);
    close(pipefd[1]);

//  based on wait_demo.c from recitation 3:
    int exit_code = -1;
    pid_t child = -1;
    child = waitpid(cpid, &exit_code, 0);
    if ((-1 == child) && (errno != ECHILD)  && (errno != EINTR )) {
        fprintf(stderr, "wait failed on error: %s\n", strerror(errno));
        return 0;
    }

//  based on wait_demo.c from recitation 3:
    int exit_code2 = -1;
    pid_t child2 = -1;
    child2 = waitpid(cpid2, &exit_code2, 0);
    if ((-1 == child2) && (errno != ECHILD)  && (errno != EINTR )) {
        fprintf(stderr, "wait failed on error: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int Executing_in_background(int index, char **arglist) {
//    based on exec_fork.c from recitation 3:
//  delete the '&' sign
    arglist[index] = NULL;

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork failed on error: %s\n", strerror(errno));
        return 0;
    }
    if (pid == 0) {
        // Child process
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s failed on error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }
//  if we got here, this is the parent process, in this case we do nothing
//  because the parent doesn't wait for children
    return 1;
}
int Output_redirecting(int index, char **arglist) {
//   based on papa_son_pipe.c file from recitation 4
    pid_t cpid;
//  delete the '>' sign
    arglist[index] = NULL;

    cpid = fork();
    if (-1 == cpid) {
        fprintf(stderr, "fork failed on error: %s\n", strerror(errno));
        return 0;
    }
//  based on fifo_writer.c file from recitation 4
    if (0 == cpid) {
        int fd = open(arglist[index+1], O_CREAT | O_WRONLY, 00777);
        if (fd == -1) {
            fprintf(stderr, "open failed on error: %s\n", strerror(errno));
            exit(1);
        }
        if (dup2(fd, 1) == -1){
            fprintf(stderr, "dup2 failed on error: %s\n", strerror(errno));
            exit(1);
        }
        // close file
        close(fd);
        if (execvp(arglist[0], arglist) == -1) {
            fprintf(stderr, "%s execvp failed on error: %s\n", arglist[0], strerror(errno));
            exit(1);
        }
    }

//  based on wait_demo.c from recitation 3:
    int exit_code2 = -1;
    pid_t child2 = -1;
    child2 = waitpid(cpid, &exit_code2, 0);
    if ((-1 == child2) && (errno != ECHILD)  && (errno != EINTR )) {
        fprintf(stderr, "wait failed on error: %s\n", strerror(errno));
        return 0;
    }

    return 1;
}


int process_arglist(int count, char** arglist){
//  first part - check what is the wanted shell functionality
    int index = 0;
    int functionality = 0;

    for (int i = 0; i < count; ++i) {
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
        return executing_commands(arglist);
    }
    if (functionality == 1){
        return Executing_in_background(index, arglist);
    }
    if (functionality == 2){
        return piping(index, arglist);
    }
    if (functionality == 3){
        return Output_redirecting(index, arglist);
    }
    return 1;
}

int finalize(void){
    return 0;
}