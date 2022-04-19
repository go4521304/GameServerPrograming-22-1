#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include "protocol.h"
#include <vector>
#include <mutex>
#include <thread>
#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;

constexpr int MAX_USER = 10;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };

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

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		_comp_type = OP_SEND;
		ZeroMemory(&_over, sizeof(_over));
		memcpy(_send_buf, packet, packet[0]);
	}
};

enum SESSION_STATE { ST_FREE, ST_ACCEPTED, ST_INGAME };

void disconnect(int c_id);

class SESSION {
	OVER_EXP _recv_over;
public:
	mutex _sl;	// session state 를 사용할 때 락을 검
	SESSION_STATE _s_state;
	int _id;
	SOCKET _socket;
	short x, y;
	char _name[NAME_SIZE];
	int _prev_remain;


public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = y = 0;
		_name[0] = 0;
		_s_state = ST_FREE;
		_prev_remain = 0;
	}
	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;

		int ret = WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
		if (0 != ret)
		{
			int err_no = WSAGetLastError();
			if (err_no != WSA_IO_PENDING)
				error_display("WSARecv : ", err_no);
		}

	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}

	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.id = _id;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.x = x;
		p.y = y;
		do_send(&p);
	}

	void send_move_packet(int c_id);
};

array<SESSION, MAX_USER> clients;
HANDLE g_h_iocp;
SOCKET g_s_socket;

void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.x = clients[c_id].x;
	p.y = clients[c_id].y;
	do_send(&p);
}


int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i)
	{
		clients[i]._sl.lock();
		if (clients[i]._s_state == ST_FREE)
		{
			clients[i]._s_state = ST_ACCEPTED;
			clients[i]._sl.unlock();
			return i;
		}
		clients[i]._sl.unlock();
	}
	return -1;
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1])
	{
	case CS_LOGIN:
	{
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		clients[c_id]._sl.lock();
		if (clients[c_id]._s_state == ST_FREE)
		{
			clients[c_id]._sl.unlock();
			break;
		}
		if (clients[c_id]._s_state == ST_INGAME)
		{
			clients[c_id]._sl.unlock();
			disconnect(c_id);
			break;
		}
		strcpy_s(clients[c_id]._name, p->name);
		clients[c_id].send_login_info_packet();

		clients[c_id]._s_state = ST_INGAME;
		clients[c_id]._sl.unlock();

		for (auto& pl : clients)
		{
			if (pl._id == c_id) continue;
			pl._sl.lock();
			if (pl._s_state != ST_INGAME)
			{
				pl._sl.unlock();
				continue;
			}
			SC_ADD_PLAYER_PACKET add_packet;
			add_packet.id = c_id;
			strcpy_s(add_packet.name, p->name);
			add_packet.size = sizeof(add_packet);
			add_packet.type = SC_ADD_PLAYER;
			add_packet.x = clients[c_id].x;
			add_packet.y = clients[c_id].y;
			pl.do_send(&add_packet);
			pl._sl.unlock();
		}

		for (auto& pl : clients)
		{
			if (pl._id == c_id) continue;
			lock_guard<mutex> aa {pl._sl};	// 락 가드가 속해 있는 블럭에서 빠져 나갈 떄 언락을 하고 빠져나감
			if (pl._s_state != ST_INGAME) continue;
			SC_ADD_PLAYER_PACKET add_packet;
			add_packet.id = pl._id;
			strcpy_s(add_packet.name, pl._name);
			add_packet.size = sizeof(add_packet);
			add_packet.type = SC_ADD_PLAYER;
			add_packet.x = pl.x;
			add_packet.y = pl.y;
			clients[c_id].do_send(&add_packet);
		}
		break;
	}

	case CS_MOVE:
	{
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		short x = clients[c_id].x;
		short y = clients[c_id].y;

		cout << "Client " << c_id << " move to " << (int)p->direction << endl;

		switch (p->direction)
		{
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		clients[c_id].x = x;
		clients[c_id].y = y;
		for (auto& pl : clients)
		{
			lock_guard<mutex> aa{ pl._sl };
			if (ST_INGAME == pl._s_state)
				pl.send_move_packet(c_id);
		}
		break;
	}
	}
}

void disconnect(int c_id)
{
	clients[c_id]._sl.lock();
	if (clients[c_id]._s_state == ST_FREE)
	{
		clients[c_id]._sl.unlock();
		return;
	}
	closesocket(clients[c_id]._socket);
	clients[c_id]._s_state = ST_FREE;
	clients[c_id]._sl.unlock();

	for (auto& pl : clients)
	{
		if (pl._id == c_id) continue;
		pl._sl.lock();
		if (pl._s_state != ST_INGAME) {
			pl._sl.unlock();
			continue;
		}
		SC_REMOVE_PLAYER_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_PLAYER;
		pl.do_send(&p);
		pl._sl.unlock();
	}
}

void do_worker()
{
	while (true)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over;
		BOOL ret = GetQueuedCompletionStatus(g_h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret)
		{
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept ERROR";
			else
			{
				cout << "GQCS Error on cleint[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}
		switch (ex_over->_comp_type)
		{
		case OP_ACCEPT:
		{
			SOCKET c_socket = reinterpret_cast<SOCKET>(ex_over->_wsabuf.buf);
			int client_id = get_new_client_id();
			if (client_id != -1)
			{
				clients[client_id].x = 0;
				clients[client_id].y = 0;
				clients[client_id]._id = client_id;
				clients[client_id]._name[0] = 0;
				clients[client_id]._socket = c_socket;
				clients[client_id]._prev_remain = 0;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), g_h_iocp, client_id, 0);
				clients[client_id].do_recv();

				c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else
			{
				cout << "Max use exceeded.\n";
			}
			ZeroMemory(&ex_over->_over, sizeof(ex_over->_over));
			ex_over->_wsabuf.buf = reinterpret_cast<CHAR*>(c_socket);
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, c_socket, ex_over->_send_buf, 0, addr_size + 16, addr_size + 16, 0, &ex_over->_over);
			break;
		}

		case OP_RECV:
		{
			if (0 == num_bytes) disconnect(key);

			int remain_date = num_bytes + clients[key]._prev_remain;	// 지난번에 처리하고 남은 패킷의 사이즈를 더함
			char* p = ex_over->_send_buf;
			while (remain_date > 0)
			{
				int packet_size = p[0];	// 패킷의 크기
				if (packet_size <= remain_date)
				{
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_date = remain_date - packet_size;
				}
				else break;
			}
			clients[key]._prev_remain = remain_date;
			if (remain_date > 0)
			{
				memcpy(ex_over->_send_buf, p, remain_date);
			}
			clients[key].do_recv();
			break;
		}

		case OP_SEND:
		{
			if (0 == num_bytes) disconnect(key);
			delete ex_over;
			break;
		}
		}
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);	// IOCP 쓰려면 2.2
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	int client_id = 0;

	g_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), g_h_iocp, 9999, 0);
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	OVER_EXP a_over;
	a_over._comp_type = OP_ACCEPT;
	a_over._wsabuf.buf = reinterpret_cast<CHAR*>(c_socket);	// 안쓰는 버퍼에 c_sockek을 집어넣음
	AcceptEx(g_s_socket, c_socket, a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &a_over._over);

	vector<thread> worker_threads;
	for (int i = 0; i < 6; ++i)
	{
		worker_threads.emplace_back(do_worker);
	}
	for (auto& th : worker_threads)
	{
		th.join();
	}


	closesocket(g_s_socket);
	WSACleanup();
}
