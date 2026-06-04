#include <stdio.h>
#include <unistd.h>
// #include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#define PRINTF(str) printf("[%s - %d] %s\n",  __func__, __LINE__, str);

void error_die(const char *message) {
	perror(message);
	exit(1);
}


int startup(unsigned short *port) {
	WSADATA data;
	int ret = WSAStartup(MAKEWORD(1, 1), &data);
	if (ret != 0) {
		printf("WSAStartup failed: %d\n", ret);
		return -1;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == INVALID_SOCKET) {
		error_die("socket failed");
	}

	// Set socket options to allow reuse of the address
	int opt = 1;
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
	if (ret != 0) {
		error_die("setsockopt failed");
	}

	// Bind the socket to the specified port
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(*port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret != 0) {
		error_die("bind failed");
	}

	// If the port was set to 0, get the assigned port number
	if(*port == 0) {
		int len = sizeof(server_addr);
		ret = getsockname(sock, (struct sockaddr *)&server_addr, &len);
		if (ret != 0) {
			error_die("getsockname failed");
		}
		*port = ntohs(server_addr.sin_port);
	}

	// Listen for incoming connections
	ret = listen(sock, 5);
	if (ret != 0) {
		error_die("listen failed");
	}

	return sock;
}

int get_line(int sock, char *buffer, int size) {
	int i = 0;
	char c = '\0';
	int n;

	while((i<size-1) && (c != '\n')) {
		n = recv(sock, &c, 1, 0);
		if(n > 0) {
			if(c == '\r') {
				// Peek at the next character without removing it from the buffer
				n = recv(sock, &c, 1, MSG_PEEK); 
				if((n > 0) && (c == '\n')) {
					recv(sock, &c, 1, 0);
				} else {
					c = '\n';
				}
			}
			buffer[i++] = c;
		} else {
			c = '\n';
		}
	}
	buffer[i] = '\0';
	return i;
}

void unimplement(int client_sock) {
	const char *response = "HTTP/1.0 501 Method Not Implemented\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<html><head><title>501 Method Not Implemented</title></head>"
		"<body><h1>501 Method Not Implemented</h1></body></html>";

	send(client_sock, response, strlen(response), 0);
}

void not_found(int client_sock) {
	const char *response = "HTTP/1.0 404 Not Found\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<html><head><title>404 Not Found</title></head>"
		"<body><h1>404 Not Found</h1></body></html>";

	send(client_sock, response, strlen(response), 0);
}

void header(int client_sock, const char *status) {
	const char *response_header = "HTTP/1.0 %s\r\n"
		"Content-Type: text/html\r\n"
		"\r\n";
	char header_buffer[1024];
	snprintf(header_buffer, sizeof(header_buffer), response_header, status);

	send(client_sock, header_buffer, strlen(header_buffer), 0);
}

void send_resource(int client_sock, FILE *resource) {
	char buffer[1024];
	size_t bytes_read;
	while((bytes_read = fread(buffer, 1, sizeof(buffer), resource)) > 0) {
		send(client_sock, buffer, bytes_read, 0);
	}
	printf("Finished sending resource %s\n", buffer);
}

void send_server_file(int client_sock, const char *filename) {
	int read_bytes = 1;
	char buffer[1024];

	// Read and discard the rest of the request headers
	// 不然会导致后续的请求被前一个请求的残留数据干扰
	while (read_bytes > 0 && strcmp(buffer, "\n")) {
		read_bytes = get_line(client_sock, buffer, sizeof(buffer));
		PRINTF(buffer);
	}

	FILE *file = fopen(filename, "rb");
	if(file == NULL) {
		not_found(client_sock);
		return;
	}

	header(client_sock, "200 OK");
	send_resource(client_sock, file);

	fclose(file);
}

DWORD WINAPI handle_request(LPVOID lpParam) {
	int client_sock = (int)(intptr_t)lpParam;

	// GET /index.html HTTP/1.1\r\n
	char buffer[1024];
	int bytes_read = get_line(client_sock, buffer, sizeof(buffer));
	PRINTF(buffer);

	char method[255];
	int j = 0, i = 0;
	while(!isspace(buffer[j]) && (i<sizeof(method)-1) ) {
		method[i++] = buffer[j++];
	}
	method[i] = '\0';
	PRINTF(method);

	if(stricmp(method, "GET") && stricmp(method, "POST")) {
		unimplement(client_sock);
		return 0;
	}

	while(isspace(buffer[j]) && (j < bytes_read)) {
		j++;
	}

	char url[255];
	int k = 0;
	while(!isspace(buffer[j]) && (k < sizeof(url)-1) && (j < bytes_read)) {
		url[k++] = buffer[j++];
	}
	url[k] = '\0';
	PRINTF(url);

	char path[512];
	snprintf(path, sizeof(path), "content%s", url);
	if(path[strlen(path)-1] == '/') {
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat st;
	if(stat(path, &st) == -1) {
		not_found(client_sock);
		return 0;
	} else {
		if((st.st_mode & S_IFMT) == S_IFDIR) {
			strcat(path, "/index.html");
		}

		PRINTF(path);

		send_server_file(client_sock, path);
	}

	closesocket(client_sock);
	return 0;
}

int main() {
	unsigned short port = 80;
	int sock = startup(&port);
	printf("Server started on port %d\n", port);

	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	while(1) {

		int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
		if(client_sock == INVALID_SOCKET) {
			error_die("accept failed");
		}

		DWORD thread_id = 0;
		void *ret = CreateThread(0, 0, handle_request, (void *)(intptr_t)client_sock, 0, &thread_id);
		if (ret == 0) {
			error_die("CreateThread failed");
		}
		
	}

	closesocket(sock);
	return 0;
}