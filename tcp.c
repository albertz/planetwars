/*
 * Connects your local bot to a remote map server for one game.
 * The map server connects random available bots on random maps,
 * updating the map and enforcing the rules.
 *
 * gcc -o tcp tcp.c
 * ./tcp 213.3.30.106 9999 username ./MyBot
 *
 * See http://www.benzedrine.cx/planetwars/ for ELO ratings.
 *
 * History
 *   2.1 20100907 kill() child
 *   2.0 20100906 rename from previous contest
 *
 * Copyright (c) 2010 Daniel Hartmeier <daniel@benzedrine.cx>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
tcp_connect(const char *host, unsigned port)
{
	int fd;
	struct sockaddr_in sa;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket: %s\n", strerror(errno));
		return (-1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(host);
	sa.sin_port = htons(port);
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa))) {
		printf("connect: %s\n", strerror(errno));
		close(fd);
		return (-1);
	}
	return (fd);
}

static pid_t
bpopen(char *cmd, int *fdr, int *fdw)
{
	int p2c[2], c2p[2];
	pid_t pid;
	char *argv[] = { NULL, NULL };

	argv[0] = cmd;
	if (pipe(p2c) || pipe(c2p)) {
		printf("pipe: %s\n", strerror(errno));
		return (0);
	}
	if ((pid = fork()) < 0) {
		printf("fork: %s\n", strerror(errno));
		return (0);
	}
	if (!pid) {
		close(c2p[0]);
		close(p2c[1]);
		dup2(c2p[1], STDOUT_FILENO);
		dup2(p2c[0], STDIN_FILENO);
		execv(argv[0], argv);
		fprintf(stderr, "execv: %s: failed\n", argv[0]);
		exit(1);
	}
	close(c2p[1]);
	close(p2c[0]);
	*fdw = p2c[1];
	*fdr = c2p[0];
	return (pid);
}

static void
split_lines(const char *s, char *d, unsigned len, int fd)
{
	unsigned off = strlen(d);
	while (*s && off + 2 < len) {
		if ((d[off] = *s++) == '\n') {
			d[off + 1] = 0;
			if (!strncmp(d, "INFO ", 5))
				printf("%s", d + 5);
			else {
				printf("%s", d);
				write(fd, d, off + 1);
			}
			*d = 0;
			off = 0;
		} else
			off++;
	}
	d[off] = 0;
}

int main(int argc, char *argv[])
{
	int fd[3] = { -1, -1, -1 };
	pid_t child = 0;
	int i, r, len;
	fd_set read_fds;
	struct timeval tv;
	char buf[1024], line[2][1024];

	if (argc != 5) {
		printf("usage: %s ip port username command\n", argv[0]);
		return (1);
	}

	if (!(child = bpopen(argv[4], &fd[1], &fd[2])))
		goto done;
	sleep(3); /* allow child to properly startup... */

	if ((fd[0] = tcp_connect(argv[1], atoi(argv[2]))) < 0)
		goto done;
	snprintf(buf, sizeof(buf), "USER %s\n", argv[3]);
	write(fd[0], buf, strlen(buf));

	printf("connected to %s:%s, waiting for game\n", argv[1], argv[2]);
	line[0][0] = line[1][0] = 0;
	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(fd[0], &read_fds);
		FD_SET(fd[1], &read_fds);
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		r = select(((fd[0] > fd[1]) ? fd[0] : fd[1]) + 1, &read_fds,
		    NULL, NULL, &tv);
		if (r < 0) {
			if (errno != EINTR) {
				printf("select: %s\n", strerror(errno));
				goto done;
			}
			continue;
		}
		if (r == 0)
			continue;
		for (i = 0; i < 2; ++i) {
			if (!FD_ISSET(fd[i], &read_fds))
				continue;
			len = read(fd[i], buf, sizeof(buf) - 1);
			if (len < 0) {
				if (errno != EINTR) {
					printf("read: %s\n", strerror(errno));
					goto done;
				}
				continue;
			}
			if (len == 0)
				goto done;
			buf[len] = 0;
			split_lines(buf, line[i], sizeof(line[i]),
			    i ? fd[0] : fd[2]);
		}
	}
	
done:
	for (i = 0; i < 3; ++i)
		if (fd[i] != -1)
			close(fd[i]);
	if (child) {
		if (kill(child, SIGKILL))
			printf("kill: %s\n", strerror(errno));
		wait(NULL);
	}
	return (0);
}
