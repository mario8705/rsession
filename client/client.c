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

int attempt_connect(struct addrinfo *p)
{
	int fd;

	fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	if (fd < 0)
	{
		return -1;
	}
	if (connect(fd, p->ai_addr, p->ai_addrlen) < 0)
	{
		close(fd);
		return -1;
	}
	return fd;
}

int resolve_and_connect(const char *addr)
{
	struct addrinfo hints;
	struct addrinfo *info;
	struct addrinfo *p;
	int status;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((status = getaddrinfo(addr, "2510", &hints, &info)) != 0)
	{
		fprintf(stderr, "Could not resolve %s: %s\n", addr, gai_strerror(status));
		return -1;
	}
	for (p = info; p; p = p->ai_next)
	{
		if ((status = attempt_connect(p)) >= 0)
			break ;
	}
	freeaddrinfo(info);
	return status;
}

void client_loop(int fd)
{
	fd_set rdset;
	char buf[256];
	ssize_t n;

	while (1)
	{
		FD_ZERO(&rdset);
		FD_SET(0, &rdset);
		FD_SET(fd, &rdset);

		if (select(fd + 1, &rdset, NULL, NULL, NULL) < 0)
		{
			perror("select");
			break ;
		}
		if (FD_ISSET(fd, &rdset))
		{
			n = read(fd, buf, sizeof(buf));
			if (n < 0)
			{
				perror("read");
				break ;
			}
			write(0, buf, n);
		}
		if (FD_ISSET(0, &rdset))
		{
			n = read(0, buf, sizeof(buf));
			if (n < 0)
			{
				perror("read");
				break ;
			}
			if (n == 0)
				break ;
			write(fd, buf, n);
		}
	}
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct sockaddr_in sin;
	struct termios raw;
	struct termios old;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <address>\n", basename(argv[0]));
		return 1;
	}

	if (!isatty(0))
	{
		fprintf(stderr, "This command only works in interactive mode\n");
		return 1;
	}

	tcgetattr(0, &raw);
	old = raw;

	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_iflag &= ~(IXON | ICRNL);
	raw.c_oflag &= ~(OPOST);	

	tcsetattr(0, TCSAFLUSH, &raw);

	if ((sockfd = resolve_and_connect(argv[1])) < 0)
	{
		fprintf(stderr, "Could not connect to %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	printf("Connected !\n");

	client_loop(sockfd);
	
	close(sockfd);
	tcsetattr(0, TCSAFLUSH, &old);
	return 0;
}

