#include <iostream>
#include <WS2tcpip.h>

using namespace std;

#pragma comment (lib, "WS2_32.LIB")
const short SERVER_PORT = 6000;
const int BUFSIZE = 256;

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);

	INT addr_size = sizeof(server_addr);

	SOCKET c_socket[2];
	DWORD set_byte;
	WSABUF setBuf;
	char buf[BUFSIZE];
	buf[0] = 0;
	setBuf.buf = buf; setBuf.len = 1;

	c_socket[0] = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, 0, 0);
	WSASend(c_socket[0], &setBuf, 1, &set_byte, 0, 0, 0);

	cout << "First Client was connected" << endl;

	c_socket[1] = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, 0, 0);
	buf[0] = 1;
	WSASend(c_socket[1], &setBuf, 1, &set_byte, 0, 0, 0);

	cout << "Second Client was connected" << endl;


	buf[0] = 2;
	WSASend(c_socket[0], &setBuf, 1, &set_byte, 0, 0, 0); 
	WSASend(c_socket[1], &setBuf, 1, &set_byte, 0, 0, 0);

	while (1)
	{
		for (int i = 0; i < 2; ++i) {
			char recv_buf[BUFSIZE];
			WSABUF mybuf;
			mybuf.buf = recv_buf; mybuf.len = BUFSIZE;
			DWORD recv_byte;
			DWORD recv_flag = 0;
			WSARecv(c_socket[i], &mybuf, 1, &recv_byte, &recv_flag, 0, 0);

			cout << i << " Client sent " << mybuf.buf[0] - '0' << endl;

			DWORD sent_byte;
			mybuf.len = recv_byte;
			WSASend(c_socket[(i + 1) % 2], &mybuf, 1, &sent_byte, 0, 0, 0);
		}
	}
	WSACleanup();
}