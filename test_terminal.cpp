#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <spawn.h>

#include <string>
// << string_format("%d", 202412);
template <typename... Args>
std::string string_format(const std::string &format, Args... args)
{
    size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args...); // Extra space for \0
    // unique_ptr<char[]> buf(new char[size]);
    char bytes[size];
    snprintf(bytes, size, format.c_str(), args...);
    return std::string(bytes);
}

extern char **environ;

#define READ_PIPE 0
#define WRITE_PIPE 1

static int g_nsh_stdin[2];
static int g_nsh_stdout[2];
static int g_nsh_stderr[2];

static void remove_escape_codes(char *buf, int len)
{
    int i;
    int j;

    for (i = 0; i < len; i++)
    {
        /* Escape Code looks like 0x1b 0x5b 0x4b */

        if (buf[i] == 0x1b)
        {
            /* Remove 3 bytes */

            for (j = i; j + 2 < len; j++)
            {
                buf[j] = buf[j + 3];
            }
        }
    }
}

static bool has_input(int fd)
{
    int ret;

    /* Poll the File Descriptor for input */

    struct pollfd fdp;
    fdp.fd = fd;
    fdp.events = POLLIN;
    ret = poll(&fdp, /* File Descriptors */
               1,    /* Number of File Descriptors */
               0);   /* Poll Timeout (Milliseconds) */

    if (ret > 0)
    {
        /* If poll is OK and there is input */

        if ((fdp.revents & POLLIN) != 0)
        {
            /* Report that there is input */

            return true;
        }

        /* Else report no input */

        return false;
    }
    else if (ret == 0)
    {
        /* If timeout, report no input */

        return false;
    }
    else if (ret < 0)
    {
        /* Handle error */

        printf("poll failed: %d, fd=%d\n", ret, fd);
        return false;
    }

    /* Never comes here */

    return false;
}

int run_cmd(char *cmd)
{

    int ret;
    pid_t pid;
    char *const argv[] = { NULL};

    /* Create the pipes for NSH Shell: stdin, stdout and stderr */

    ret = pipe(g_nsh_stdin);
    if (ret < 0)
    {
        printf("stdin pipe failed: %d\n", errno);
        return -1;
    }

    ret = pipe(g_nsh_stdout);
    if (ret < 0)
    {
        printf("stdout pipe failed: %d\n", errno);
        return -1;
    }

    ret = pipe(g_nsh_stderr);
    if (ret < 0)
    {
        printf("stderr pipe failed: %d\n", errno);
        return -1;
    }

    /* Close default stdin, stdout and stderr */

    close(0);
    close(1);
    close(2);

    /* Assign the new pipes as stdin, stdout and stderr */

    dup2(g_nsh_stdin[READ_PIPE], 0);
    dup2(g_nsh_stdout[WRITE_PIPE], 1);
    dup2(g_nsh_stderr[WRITE_PIPE], 2);

    /* Start the NSH Shell and inherit stdin, stdout and stderr */

    ret = posix_spawn(&pid,      /* Returned Task ID */
                      "/bin/sh", /* NSH Path */
                      NULL,      /* Inherit stdin, stdout and stderr */
                      NULL,      /* Default spawn attributes */
                      argv,      /* No arguments */
                      NULL);     /* No environment */
    if (ret < 0)
    {
        int errcode = errno;
        printf("posix_spawn failed: %d\n", errcode);
        return -errcode;
    }

    while (1)
    {
        // 延时一秒
        sleep(1);
        // 接收键盘输入并写入管道
        char buf[1024] = "date && date > input.log\r\n";
        int len = strlen(buf);
        // int len = read(0, buf, sizeof(buf));
        if (len > 0)
        {
            printf("len: %d %s\n", len, buf);
            ret = write(g_nsh_stdin[WRITE_PIPE], buf, len);
            if (ret < 0)
            {
                printf("write failed: %d\n", errno);
                return -1;
            }
        };

        if (has_input(g_nsh_stdout[READ_PIPE]))
        {
            /* Read the output from NSH stdout */

            ret = read(g_nsh_stdout[READ_PIPE], buf, sizeof(buf) - 1);
            if (ret > 0)
            {
                /* Add to NSH Output Text Area */

                buf[ret] = 0;
                remove_escape_codes(buf, ret);
                system(string_format(" echo 'g_nsh_stdout %s' > output.log\n", buf).c_str());
            }
        }

        /* Poll NSH stderr to check if there's output to be processed */

        if (has_input(g_nsh_stderr[READ_PIPE]))
        {
            /* Read the output from NSH stderr */

            ret = read(g_nsh_stderr[READ_PIPE], buf, sizeof(buf) - 1);
            if (ret > 0)
            {
                /* Add to NSH Output Text Area */

                buf[ret] = 0;
                remove_escape_codes(buf, ret);
                // auto tmp = string_format("g_nsh_stderr %s\n", buf);
                system(string_format(" echo 'g_nsh_stderr %s' > output.log\n", buf).c_str());
            }
        }
    }

    // pid_t pid;
    // char *argv[] = {"sh", "-c", cmd, NULL};
    // int status;
    // printf("Run command: %s\n", cmd);
    // status = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
    // if (status == 0) {
    //     printf("Child pid: %i\n", pid);
    //     if (waitpid(pid, &status, 0) != -1) {
    //         printf("Child exited with status %i\n", status);
    //     } else {
    //         perror("waitpid");
    //     }
    // } else {
    //     printf("posix_spawn: %s\n", strerror(status));
    // }

    return 0;
}

int main(int argc, char *argv[])
{
    run_cmd(argv[1]);
    return 0;
}