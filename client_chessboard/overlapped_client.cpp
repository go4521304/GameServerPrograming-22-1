#include <iostream>
#include <WS2tcpip.h>
#include <Windows.h>
#include <conio.h>
#include <unordered_map>

using namespace std;

#pragma comment (lib, "WS2_32.LIB")
const short SERVER_PORT = 8000;
const int BUFSIZE = 200;

constexpr int UP = 72;
constexpr int DOWN = 80;
constexpr int LEFT = 75;
constexpr int RIGHT = 77;

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

void gotoxy(int x, int y)
{
	COORD pos = { x,y };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}
bool ChangePos(const char input);

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

char recv_buf[BUFSIZE];
SOCKET s_socket;
WSABUF mybuf_r;

char send_buf[BUFSIZE];
WSABUF s_wsabuf;

void process_packet(int length);
void request_login();
void do_send();
void do_recv(SOCKET s_socket);

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag);
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED lp_over, DWORD s_flag);

void onDraw();
void clearDraw(int id);

struct player_info
{
	int id;
	char ICON[3];
	short x, y;
};

int player_id = -1;
int player_num = 0;

unordered_map<int, player_info> pInfo;

int main()
{
	char SERVER_ADDR[20];

	cout << "Enter Server IP: ";
	cin >> SERVER_ADDR;

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
	request_login();


	for (;;)
	{
		if (_kbhit())
		{
			do_send();
		}
		else
		{
			onDraw();
		}
		SleepEx(30, true); // 실제 게임에서는 Rendering Loop
	}
	WSACleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void request_login()
{
	cs_packet_login packet;
	packet.type = LOGIN;
	packet.size = sizeof(packet);

	s_wsabuf.buf = reinterpret_cast<char*>(&packet);
	s_wsabuf.len = static_cast<ULONG>(packet.size);

	WSAOVERLAPPED* s_over = new WSAOVERLAPPED;
	ZeroMemory(s_over, sizeof(WSAOVERLAPPED));
	WSASend(s_socket, &s_wsabuf, 1, 0, 0, s_over, send_callback);
}

void do_send()
{
	char cmd;
	cmd = _getch();
	clearDraw(player_id);

	ChangePos(cmd);

	// make packet
	cs_packet_move packet;
	packet.id = player_id;
	packet.type = MOVE;
	packet.x = pInfo[player_id].x;
	packet.y = pInfo[player_id].y;
	packet.size = sizeof(packet);
	char* a = reinterpret_cast<char*>(&packet);
	memcpy(send_buf, reinterpret_cast<char*>(&packet), sizeof(packet));
	//

	s_wsabuf.buf = send_buf;
	s_wsabuf.len = sizeof(packet);

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
	char* m_start = recv_buf;
	int length = num_bytes;

	while (true) {
		int message_size = m_start[0];
		num_bytes -= message_size;
		if (0 >= num_bytes) break;
		m_start += message_size;
	}
	process_packet(length);

	delete lp_over;
	do_recv(s_socket);
	return;
}

////////////////////////////////////////////////////////////////////////////////////////
void process_packet(int length)
{
	int num = 0;
	while (1)
	{
		char tmp_buf[BUFSIZE];
		memcpy(tmp_buf, recv_buf + num, recv_buf[0] + num);
		num += tmp_buf[0];
		if (tmp_buf[1] == LOGIN_OK)
		{
			sc_packet_player* packet = reinterpret_cast<sc_packet_player*>(tmp_buf);
			player_info p;
			p.id = packet->id;
			p.x = packet->x;
			p.y = packet->y;
			memcpy(p.ICON, packet->icon, sizeof(packet->icon));
			pInfo.try_emplace(packet->id, p);

			if (player_id == -1)
			{
				player_id = packet->id;

				gotoxy(0, 0);
				cout << "   A   B   C   D   E   F   G   H" << endl;
				cout << " ┌ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┐" << endl;
				for (int i = 0; i < 8; ++i)
				{
					cout << i << "│";
					for (int j = 0; j < 8; ++j)
					{
						cout << "   │";
					}
					if (i < 7)
					{
						cout << "\n ├ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┤" << endl;
					}
				}
				cout << "\n └ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┘" << endl;
				cout << "Use WSAD to move" << endl;
				cout << "You are" << packet->icon << "Input your Action" << endl;
			}
		}
		else if (tmp_buf[1] == PLAYER)
		{
			sc_packet_player* packet = reinterpret_cast<sc_packet_player*>(tmp_buf);
			player_info p;
			p.id = packet->id;
			p.x = packet->x;
			p.y = packet->y;
			memcpy(p.ICON, packet->icon, sizeof(packet->icon));
			pInfo.try_emplace(packet->id, p);

		}
		else if (tmp_buf[1] == MOVE)
		{
			sc_packet_move* packet = reinterpret_cast<sc_packet_move*>(tmp_buf);
			clearDraw(packet->id);
			pInfo[packet->id].x = packet->x;
			pInfo[packet->id].y = packet->y;
		}

		else if (tmp_buf[1] == DISCONNECT)
		{
			sc_packet_delect* packet = reinterpret_cast<sc_packet_delect*>(tmp_buf);
			clearDraw(packet->id);
			pInfo.erase(packet->id);
		}

		if (num == length)
			break;
	}
}

void onDraw()
{
	// X = 3 + pos.x * 4
	// Y = 2 + pos.y * 2
	// gotoxy(0, 20) 에서 입력

	for (auto i : pInfo)
	{
		gotoxy((i.second.x * 4 + 3), (i.second.y * 2 + 2));
		cout << i.second.ICON;
	}
	gotoxy(0, 20);
	cout << "Enter Command: ";
}

void clearDraw(int id)
{
	gotoxy((pInfo[id].x * 4 + 3), (pInfo[id].y * 2 + 2));
	cout << "  ";
}

////////////////////////////////////////////////////////////////////////////////////////
bool ChangePos(const char input)
{
	switch (input)
	{
	case UP:
		if (pInfo[player_id].y > 0)
		{
			pInfo[player_id].y--;
		}
		break;

	case DOWN:
		if (pInfo[player_id].y < 7)
		{
			pInfo[player_id].y++;
		}
		break;

	case LEFT:
		if (pInfo[player_id].x > 0)
		{
			pInfo[player_id].x--;
		}
		break;

	case RIGHT:
		if (pInfo[player_id].x < 7)
		{
			pInfo[player_id].x++;
		}
		break;

	case 'q':
	case 'Q':
		exit(0);
		WSACleanup();
		break;

	default:
		return false;
		break;
	}
	return true;
}