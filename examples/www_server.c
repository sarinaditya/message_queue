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

static int open_http_listener();
static void handle_client(int fd);
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

static void handle_client(int fd) {
	char buf[1024];
	int pos = 0, r;
	while((r = read(fd, buf+pos, 1024-pos)) > 0) {
		pos += r;
		if(pos >= 4 && !strncmp(buf+pos-4, "\r\n\r\n", 4)) {
			buf[pos] = '\0';
			handle_client_request(fd, buf);
			return;
		}
	}
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
			write(fd, buf, strlen(buf));
			while(copy_data(rfd, fd) > 0);
			close(rfd);
			close(fd);
			return;
		}
	}
	char buf[39];
	snprintf(buf, 39, "HTTP/1.0 %s\r\n\r\n", errno_to_http_status());
	buf[38] = '\0';
	write(fd, buf, strlen(buf));
	close(rfd);
	close(fd);
}

static int copy_data(int rfd, int fd) {
	char buf[1024];
	ssize_t r = read(rfd, buf, 1024);
	if(r)
		write(fd, buf, r);
	return r;
}

int main(int argc, char *argv[]) {
	int fd = open_http_listener();
	if(fd >= 0) {
		while(1) {
			struct sockaddr_in peer_addr;
			socklen_t peer_len = sizeof(peer_addr);
			int cfd = accept(fd, (struct sockaddr *)&peer_addr, &peer_len);
			if(cfd >= 0) {
				handle_client(cfd);
			}
		}
	} else {
		perror("Error listening on *:8080");
	}
}
