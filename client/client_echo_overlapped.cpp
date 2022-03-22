#include <iostream>
#include <WS2tcpip.h>

using namespace std;

#pragma comment (lib, "WS2_32.LIB")

const char* SERVER_ADDR = "127.0.0.1";
const short SERVER_PORT = 6000;
const int BUFSIZE = 256;

char recv_buf[BUFSIZE];
SOCKET s_socket;
WSABUF mybuf_r;

char send_buf[BUFSIZE];
WSABUF s_wsabuf;

void do_send();
void do_recv(SOCKET s_socket);

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag);
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag);

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러 " << lpMsgBuf << std::endl;
	while (true);
	LocalFree(lpMsgBuf);
}


int main()
{
	wcout.imbue(locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
	connect(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	do_recv(s_socket);
	do_send();
	for (;;) {
		SleepEx(10, true); // 실제 게임에서는 Rendering Loop
	}
	WSACleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void do_send()
{
	cout << "Enter Message : ";
	cin.getline(send_buf, BUFSIZE);
	s_wsabuf.buf = send_buf; s_wsabuf.len = static_cast<ULONG>(strlen(send_buf)) + 1;

	WSAOVERLAPPED* s_over = new WSAOVERLAPPED;
	ZeroMemory(s_over, sizeof(WSAOVERLAPPED));
	WSASend(s_socket, &s_wsabuf, 1, 0, 0, s_over, send_callback);
}

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag)
{
	delete lp_over;
	return;
}


void do_recv(SOCKET s_socket)
{
	mybuf_r.buf = recv_buf;
	mybuf_r.len = BUFSIZE;
	DWORD recv_flag = 0;
	WSAOVERLAPPED* r_over = new WSAOVERLAPPED;
	ZeroMemory(r_over, sizeof(WSAOVERLAPPED));
	int ret = WSARecv(s_socket, &mybuf_r, 1, 0, &recv_flag, r_over, recv_callback);

	if (0 != ret)
	{
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING)
		{
			error_display("WSARecv : ", err_no);
		}
	}
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag)
{
	cout << "Server Sent [" << num_bytes << "bytes] : " << recv_buf << endl;
	delete lp_over;
	do_recv(s_socket);
	do_send();
	return;
}
