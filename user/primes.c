#include "../kernel/types.h"
#include "./user.h"

void process(int p_read, int p_write)
{
    close(p_write);
    int prime;
    if (read(p_read, &prime, 4) != 0)
    {
        fprintf(1, "prime %d\n", prime);
        int p1[2];
        pipe(p1);
        if (fork() == 0)
        {
            process(p1[0], p1[1]);
        }
        else
        {
            int num;
            close(p1[0]);
            while (read(p_read, &num, 4) != 0)
            {
                if (num % prime != 0)
                {
                    write(p1[1], &num, 4);
                }
            }
            close(p_read);
            close(p1[1]);
            wait(0);
        }
        }
    else
    {
        close(p_read);
        exit(0);
    }
}

int main()
{
    int p[2];
    pipe(p);
    if (fork() == 0)
    {
        process(p[0], p[1]);
    }
    else
    {
        close(p[0]);
        for (int i = 2; i < 36; ++i)
        {
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}