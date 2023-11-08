#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <unistd.h>
#include <netdb.h>
#include <termios.h>
#include <sys/select.h>
#include <pty.h>

void client_loop(int fd)
{
    int master;
    char name[256];
    pid_t pid;
    const char *shell;

    shell = getenv("SHELL");
    if (!shell)
        shell = "/bin/sh";
    pid = forkpty(&master, name, NULL, NULL);
    if (pid < 0)
    {
        perror("forkpty");
        return ;
    }
    if (pid == 0)
    {
        char *argv[2];
        char *envp[2];

        argv[0] = basename((char *)shell);
        argv[1] = NULL;

        envp[0] = "TERM=xterm-256color";
        envp[1] = NULL;

        printf("Running %s\n", shell);
        if (execve(shell, argv, envp) < 0)
        {
            perror("execve");
        }
        exit(1);
    }

    fd_set rdset;
    char buf[256];
    ssize_t n;

    while (1)
    {
        FD_ZERO(&rdset);
        FD_SET(master, &rdset);
        FD_SET(fd, &rdset);

        if (select(master < fd ? fd + 1 : master + 1, &rdset, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break ;
        }

        if (FD_ISSET(master, &rdset))
        {
            n = read(master, buf, sizeof(buf));
            if (n < 0)
            {
                perror("read");
                break ;
            }
            write(fd, buf, n);
        }

        if (FD_ISSET(fd, &rdset))
        {
            n = read(fd, buf, sizeof(buf));
            if (n < 0)
            {
                perror("read");
                break ;
            }
            write(master, buf, n);
        }
    }
    close(fd);
    close(master);
}

void run_client(int fd)
{
    pid_t p;

    p = fork();
    if (p < 0)
    {
        perror("fork");
        close(fd);
        return ;
    }
    if (p == 0)
    {
        client_loop(fd);
        exit(0);
    }
}

void server_loop(int sockfd)
{
    socklen_t socklen;
    struct sockaddr_in sin;
    int fd;

    while (1)
    {
        socklen = sizeof(struct sockaddr_in);
        fd = accept(sockfd, (struct sockaddr *)&sin, &socklen);
        if (fd < 0)
        {
            perror("accept");
            break ;
        }
        run_client(fd);
    }
}

int main(int argc, char *argv[])
{
	int sockfd;
    struct sockaddr_in sin;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(2510);

    int val = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return 1;
    }

    server_loop(sockfd);

    close(sockfd);
	return 0;
}

