/*
 * Copyright (c) 2012 Jeremy Pepper
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of message_queue nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is an example of how to use the message queue. This is NOT a
 * production-ready Web server, or even a good example of how to write a Web
 * server. Please ignore the HTTP bits and focus on the message queue.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>

// Utility functions

static const char *errno_to_http_status() {
	switch(errno) {
	case EACCES:
		return "403 Permission Denied";
	case EMFILE:
		return "503 Service Unavailable";
	case ENOENT:
		return "404 File Not Found";
	default:
		return "500 Internal Server Error";
	}
}

static int is_valid_filename_char(char c) {
	return (c >= '0' && c <= '9') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z') ||
	       (c == '.') || (c == '_');
}

// HTTP request processing pipeline

struct client_state {
	enum {CLIENT_INACTIVE, CLIENT_READING, CLIENT_WRITING} state;
	int close_pending;
	int rfd;
	char buf[1024];
	int pos, len;
};

struct client_state client_data[FD_SETSIZE];

static int do_write(int fd, char *data, int len) {
	if(client_data[fd].state == CLIENT_WRITING) {
		len = len > 1024 - client_data[fd].len ? 1024 - client_data[fd].len : len;
		memcpy(client_data[fd].buf+client_data[fd].len, data, len);
		client_data[fd].len += len;
		return len;
	} else {
		len = len > 1024 ? 1024 : len;
		memcpy(client_data[fd].buf, data, len);
		client_data[fd].len = len;
		client_data[fd].pos = 0;
		client_data[fd].state = CLIENT_WRITING;
		return len;
	}
}

static int open_http_listener();
static void handle_client_data(int fd);
static void handle_client_request(int fd, char *request);
static void generate_client_reply(int fd, const char *filename);
static int copy_data(int rfd, int fd);

static int open_http_listener() {
	int fd = socket(PF_INET, SOCK_STREAM, 0);
	if(fd >= 0) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);
		addr.sin_addr.s_addr = INADDR_ANY;
		if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
			close(fd);
			return -1;
		}
		if(listen(fd, 128)) {
			close(fd);
			return -1;
		}
	}
	return fd;
}

static void handle_client_data(int fd) {
	int r;
	if((r = read(fd, client_data[fd].buf+client_data[fd].pos, 1024-client_data[fd].pos)) > 0) {
		client_data[fd].pos += r;
		if(client_data[fd].pos >= 4 && !strncmp(client_data[fd].buf+client_data[fd].pos-4, "\r\n\r\n", 4)) {
			client_data[fd].buf[client_data[fd].pos] = '\0';
			client_data[fd].state = CLIENT_INACTIVE;
			handle_client_request(fd, client_data[fd].buf);
			return;
		}
	}
	client_data[fd].state = CLIENT_INACTIVE;
	close(fd);
}

static void handle_client_request(int fd, char *request) {
	if(!strncmp(request, "GET /", 5)) {
		char *filename = request+5;
		char *filename_end = filename;
		while(is_valid_filename_char(*filename_end))
			++filename_end;
		*filename_end = '\0';
		generate_client_reply(fd, filename);
	}
}

static void generate_client_reply(int fd, const char *filename) {
	int rfd = open(filename, O_RDONLY);
	if(rfd >= 0) {
		struct stat st;
		if(!fstat(rfd, &st)) {
			char buf[84];
			snprintf(buf, 84, "HTTP/1.0 200 OK\r\nContent-type: text/ascii\r\nContent-Length: %lu\r\n\r\n", (unsigned long)st.st_size);
			buf[83] = '\0';
			client_data[fd].rfd = rfd;
			do_write(fd, buf, strlen(buf));
			return;
		}
	}
	char buf[39];
	snprintf(buf, 39, "HTTP/1.0 %s\r\n\r\n", errno_to_http_status());
	buf[38] = '\0';
	do_write(fd, buf, strlen(buf));
	close(rfd);
	client_data[fd].close_pending = 1;
}

static int copy_data(int rfd, int fd) {
	char buf[1024];
	ssize_t r = read(rfd, buf, 1024);
	if(r)
		do_write(fd, buf, r);
	return r;
}

int main(int argc, char *argv[]) {
	int fd = open_http_listener();
	if(fd >= 0) {
		while(1) {
			fd_set rfds, wfds;
			int max_fd, r;
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			max_fd = 0;
			FD_SET(fd, &rfds);
			for(int i=0;i<FD_SETSIZE;++i) {
				if(client_data[i].state == CLIENT_READING) {
					FD_SET(i, &rfds);
					max_fd = i;
				} else if(client_data[i].state == CLIENT_WRITING) {
					FD_SET(i, &wfds);
					max_fd = i;
				}
			}
			max_fd = fd > max_fd ? fd : max_fd;
			r = select(max_fd+1, &rfds, &wfds, NULL, NULL);
			if(r < 0 && errno != EINTR) {
				perror("Error in select");
				return -1;
			}
			if(r > 0) {
				if(FD_ISSET(fd, &rfds)) {
					struct sockaddr_in peer_addr;
					socklen_t peer_len = sizeof(peer_addr);
					int cfd = accept(fd, (struct sockaddr *)&peer_addr, &peer_len);
					if(cfd >= 0) {
						int flags = fcntl(cfd, F_GETFL, 0);
						fcntl(fd, F_SETFL, flags | O_NONBLOCK);
						client_data[cfd].state = CLIENT_READING;
						client_data[cfd].close_pending = 0;
						client_data[cfd].pos = 0;
					}
				}
				for(int i=0;i<FD_SETSIZE;++i) {
					if(i != fd && FD_ISSET(i, &rfds)) {
						handle_client_data(i);
					} else if(i != fd && FD_ISSET(i, &wfds)) {
						int r = write(i, client_data[i].buf+client_data[i].pos, client_data[i].len-client_data[i].pos);
						if(r >= 0) {
							client_data[i].pos += r;
							if(client_data[i].pos == client_data[i].len) {
								client_data[i].state = CLIENT_INACTIVE;
								if(client_data[i].close_pending) {
									close(i);
								} else {
									if(copy_data(client_data[i].rfd, i) <= 0) {
										close(client_data[i].rfd);
										close(i);
									}
								}
							}
						}
					}
				}
			}
		}
	} else {
		perror("Error listening on *:8080");
	}
}
