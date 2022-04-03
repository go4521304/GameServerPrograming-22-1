#include <iostream>
#include <unordered_map>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
using namespace std;
constexpr int PORT_NUM = 8000;
constexpr int BUF_SIZE = 200;

class SESSION;
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);
void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);

void process_packet(int client_id, char* mess);

unordered_map<int, SESSION> clients;
unordered_map<WSAOVERLAPPED*, int> over_to_session;


constexpr char ICON[10][3] = { {"グ"}, {"ケ"}, {"ゲ"}, {"コ"}, {"ゴ"}, {"サ"}, {"ザ"}, {"シ"}, {"シ"}, {"ス"} };
short player_pos[10][2] = { (0,0) };


enum PAKCET
{
	LOGIN, LOGIN_OK, PLAYER, MOVE, DISCONNECT
};

#pragma pack(push, 1)
struct cs_packet_login
{
	char size;
	char type;
};

struct cs_packet_move
{
	char size;
	char type;
	int id;
	short x, y;
};

struct sc_packet_player
{
	char size;
	char type;
	int id;
	short x, y;
	char icon[3];
};
struct sc_packet_move
{
	char size;
	char type;
	int id;
	short x, y;
};
struct sc_packet_delect
{
	char size;
	char type;
	int id;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
class SEND_DATA {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	SEND_DATA(char* n_data, int size)
	{
		_wsabuf.len = size;
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		memcpy(_send_buf, n_data, size);
	}
};

class SESSION
{
private:
	WSAOVERLAPPED _c_over;
	WSABUF _c_wsabuf[1];

public:
	int _id;
	SOCKET _socket;
	CHAR _c_mess[BUF_SIZE];
public:
	SESSION() {}
	SESSION(int id, SOCKET s) : _id(id), _socket(s)
	{
		_c_wsabuf[0].buf = _c_mess;
		_c_wsabuf[0].len = sizeof(_c_mess);
		over_to_session[&_c_over] = id;
	}
	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_c_over, 0, sizeof(_c_over));
		WSARecv(_socket, _c_wsabuf, 1, 0, &recv_flag, &_c_over, recv_callback);
	}

	void do_send(int num_bytes, char* mess)
	{
		SEND_DATA* sdata = new SEND_DATA{ mess, num_bytes };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, send_callback);
	}
};


int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	int client_id = 0;

	while (true)
	{
		SOCKET client = WSAAccept(server, reinterpret_cast<sockaddr*>(&cl_addr), &addr_size, 0, 0);
		clients.try_emplace(client_id, client_id, client);
		clients[client_id++].do_recv();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	SEND_DATA* sdata = reinterpret_cast<SEND_DATA*>(over);
	delete sdata;
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	int  client_id = over_to_session[over];
	if (0 == num_bytes) {
		cout << "Client disconnected\n";
		clients.erase(client_id);
		over_to_session.erase(over);
		char disconnect[2] = { 0, DISCONNECT };
		process_packet(client_id, disconnect);
		return;
	};

	process_packet(client_id, clients[client_id]._c_mess);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void process_packet(int client_id, char* mess)
{
	char s_mess[BUF_SIZE];
	int size;

	if (mess[1] == LOGIN)
	{
		sc_packet_player s_packet;
		s_packet.type = LOGIN_OK;
		s_packet.id = client_id;
		s_packet.x = player_pos[client_id][0];
		s_packet.y = player_pos[client_id][1];
		memcpy(s_packet.icon, ICON[client_id], sizeof(ICON[client_id]));
		s_packet.size = sizeof(s_packet);

		memcpy(s_mess, reinterpret_cast<char*>(&s_packet), sizeof(s_packet));
		size = s_packet.size;

		for (auto& cl : clients)
			cl.second.do_send(size, s_mess);

		// packet log
		cout << "Client " << client_id << "(" << ICON[client_id] << ")" << " Login" << endl;

		for (int i = 0; i < client_id; ++i)
		{
			s_packet.type = PLAYER;
			s_packet.id = i;
			s_packet.x = player_pos[i][0];
			s_packet.y = player_pos[i][1];
			memcpy(s_packet.icon, ICON[i], sizeof(ICON[i]));

			memcpy(s_mess, reinterpret_cast<char*>(&s_packet), sizeof(s_packet));
			size = s_packet.size;

			clients[client_id].do_send(size, s_mess);
		}
	}

	else if (mess[1] == MOVE)
	{
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(clients[client_id]._c_mess);
		sc_packet_move s_packet;
		s_packet.type = MOVE;
		s_packet.id = client_id;
		s_packet.x = packet->x;
		s_packet.y = packet->y;
		s_packet.size = sizeof(s_packet);

		memcpy(s_mess, reinterpret_cast<char*>(&s_packet), sizeof(s_packet));
		size = s_packet.size;

		for (auto& cl : clients)
			cl.second.do_send(size, s_mess);


		player_pos[client_id][0] = packet->x;
		player_pos[client_id][1] = packet->y;

		// packet log
		cout << "Client " << client_id << "(" << ICON[client_id] << ")" << " move to " << player_pos[client_id][0] << ", " << player_pos[client_id][1] << endl;
	}

	else if (mess[1] == DISCONNECT)
	{
		sc_packet_delect s_packet;
		s_packet.type = DISCONNECT;
		s_packet.id = client_id;
		s_packet.size = sizeof(s_packet);

		memcpy(s_mess, reinterpret_cast<char*>(&s_packet), sizeof(s_packet));
		size = s_packet.size;

		for (auto& cl : clients)
			cl.second.do_send(size, s_mess);
		return;
	}

	clients[client_id].do_recv();
}