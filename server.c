#include <stdio.h>
#include <unistd.h>
// #include <sys/socket.h>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#define PRINTF(str) printf("[%s - %d] %s",  __func__, __LINE__, str);

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

DWORD WINAPI handle_request(LPVOID lpParam) {
	int client_sock = (int)(intptr_t)lpParam;

	char buffer[1024];
	int bytes_read = get_line(client_sock, buffer, sizeof(buffer));
	PRINTF(buffer);


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