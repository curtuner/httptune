#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

const int kMaxFdNum = 100;

// 使用perror打印错误并终止进程
void error_die(const char* msg) {
    perror(msg);
    exit(1);
}

// 这里规定这个函数只会返回有效值，异常在其中处理
int open_server_sock(int port) {
    int servfd = socket(AF_INET, SOCK_STREAM, 0);
    if (servfd < 0) {
	error_die("socket");
    }

    int optval = 1;
    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
	error_die("setsockopt failed");
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)port);

    if (bind(servfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
	error_die("bind");
    }

    if (listen(servfd, 5) < 0) {
	error_die("listen");
    }

    return servfd;
}


void usage() {
    fprintf(stderr, "usage: httptune <port>\n");
}

// 僵死进程的回收
void httptune_child(int sig) {
    int wstat;
    waitpid(0, &wstat, WNOHANG);
}

// 默认
int main(int argc, char *argv[argc])
{
    if (argc != 2) {
	usage();
	return -1;
    }

    signal(SIGCHLD, httptune_child);

    int port = atoi(argv[1]);
    if (port <= 0) {
	fprintf(stderr, "port is invalid\n");
    }
    int server_sock = open_server_sock(port);


    struct pollfd fds[kMaxFdNum];
    int max_fd = 0;
    fds[0].fd = server_sock;
    fds[0].events = POLLIN;
    max_fd = 1;
    
    for (;;) {
	poll(fds, max_fd, -1);

	if (fds[0].revents & POLLIN) {
	    struct sockaddr_in clientaddr;
	    socklen_t clientlen = sizeof(clientaddr);
	    int client_sock = accept(server_sock, (struct sockaddr*)&clientaddr, &clientlen);
	    if (client_sock < 0) {
		error_die("accept");
	    }

	    if (max_fd + 1 > kMaxFdNum) {
		error_die("max_fd");
	    } else {
		fds[max_fd].fd = client_sock;
		fds[max_fd].events = POLLRDNORM;
		max_fd++;
	    }
	}
	
	for (int i = 1; i < max_fd; i++) {
	    if (fds[i].revents & POLLRDNORM) {
		char buf[1024];
		read(fds[i].fd, buf, 1024);
		if (write(fds[i].fd, "Hello", 5) < 0) {
		    error_die("write");
		}
	    }
	}
	
    }
    
    return 0;
}
