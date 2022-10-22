#include "../kernel/types.h"
#include "../kernel/fs.h"
#include "../kernel/stat.h"
#include "./user.h"

char *
fmtname(char *path) // return file name of path
{
    static char buf[DIRSIZ + 1];
    char *p = path + strlen(path);
    // Find first character after last slash.
    while (p >= path && *p != '/')
    {
        --p;
    }
    ++p;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
    {
        return p;
    }
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), 0, DIRSIZ - strlen(p));
    return buf;
}

void find(char *path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE: // 打开的路径是文件
        if (strcmp(fmtname(path), target) == 0) // 当前文件名和要查找的文件名相同
        {
            printf("%s\n", path);
        }
        break;

    case T_DIR: // 打开的路径是目录
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
        {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            if (fmtname(buf)[0] != '.')
            {
                find(buf, target);
            }
        }
        break;
    default:
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc == 2)
    {
        find(".", argv[1]);
        exit(0);
    }
    else if (argc == 3)
    {
        find((char *)argv[1], (char *)argv[2]);
        exit(0);
    }
    exit(0);
}