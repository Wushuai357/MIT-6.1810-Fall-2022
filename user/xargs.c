#include "../kernel/types.h"
#include "../kernel/param.h"
#include "./user.h"

#define BUFSIZE 512

int main(int argc, char *argvs[])
{
    sleep(3);
    char buf[BUFSIZE];
    read(0, buf, BUFSIZE);

    char *xargvs[MAXARG];
    int xargc = 0;
    for (int i = 1; i < argc; ++i) {
        xargvs[xargc++] = argvs[i];
    }

    char *p = buf;
    for (int i = 0; i < BUFSIZE; ++i) {
        if (buf[i]== '\n') {
            if (fork() == 0) {
                buf[i] = 0;
                xargvs[xargc++] = p;
                xargvs[xargc++] = 0;
                exec(xargvs[0], xargvs);
                exit(1);
            } else {
                p = &buf[i + 1];
                wait(0);
            }
        }
    }
    wait(0);
    exit(0);
}
