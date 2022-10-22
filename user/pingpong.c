#include "../kernel/types.h"
#include "./user.h"

int
main (void) {
    int p1[2];
    int p2[2];
    char buf[1];
    char msg[1];
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error!\n");
        exit(1);
    }
    if (pid == 0) { // child process
        pipe(p2);
        close(p1[1]);
        close(p2[0]);
        read(p1[0], buf, 1);
        close(p1[0]);
        fprintf(1, "%d: received ping\n", getpid());
        write(p2[1], msg, 1);
        close(p2[1]);
    } else { // parent process
        pipe(p1);
        close(p1[0]);
        close(p2[1]);
        write(p1[1], buf, 1);
        close(p1[1]);
        wait(&pid);
        read(p2[0], msg, 1);
        close(p2[0]);
        fprintf(1, "%d: received pong\n", getpid());
    }
    exit(0);
}