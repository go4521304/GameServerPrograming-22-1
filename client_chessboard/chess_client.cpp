#include <iostream>
#include <WS2tcpip.h>
#include <conio.h>

using namespace std;

#pragma comment (lib, "WS2_32.LIB")
const short SERVER_PORT = 6000;
const int BUFSIZE = 256;


constexpr int UP = 72;
constexpr int DOWN = 80;
constexpr int LEFT = 75;
constexpr int RIGHT = 77;

bool ChangePos(pair<int, int> &pos, const char input)
{
	switch (input)
	{
	case UP:
		if (pos.first > 0)
		{
			pos.first--;
		}
		break;

	case DOWN:
		if (pos.first < 7)
		{
			pos.first++;
		}
		break;

	case LEFT:
		if (pos.second > 0)
		{
			pos.second--;
		}
		break;

	case RIGHT:
		if (pos.second < 7)
		{
			pos.second++;
		}
		break;

	case 'Q':
	case 'q':
		exit(0);
		break;

	default:
		return false;
		break;
	}
	return true;
}

int main()
{
	pair<int, int> pos[2] = { make_pair<int, int>(0, 0) , make_pair<int, int>(7, 7)};
	int player, turn = 0;
	char input;
	char SERVER_ADDR[20];

	cout << "Enter Server IP: ";
	cin >> SERVER_ADDR;

	// Connect
	wcout.imbue(locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
	connect(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

	// Buffer
	WSABUF mybuf_r;
	char buf[BUFSIZE];
	DWORD recv_byte;
	DWORD recv_flag = 0;

	mybuf_r.buf = buf; mybuf_r.len = BUFSIZE;
	WSARecv(s_socket, &mybuf_r, 1, &recv_byte, &recv_flag, 0, 0);

	player = mybuf_r.buf[0];

	for (;;)
	{
		WSARecv(s_socket, &mybuf_r, 1, &recv_byte, &recv_flag, 0, 0);

		if (mybuf_r.buf[0] == 2)
			break;
	}

	// Loop
	while (1)
	{
		system("cls");
		if (player == 0)
			cout << "You: " << "★" << endl;
		else
			cout << "You: " << "◆" << endl;

		cout << "   A   B   C   D   E   F   G   H" << endl;
		cout << " ┌ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┐" << endl;
		for (int i = 0; i < 8; ++i)
		{
			cout << i + 1 << "│";
			for (int j = 0; j < 8; ++j)
			{
				if (pos[0].first == i && pos[0].second == j)
				{
					cout << " ★│";
				}
				else if (pos[1].first == i && pos[1].second == j)
				{
					cout << " ◆│";
				}
				else
				{
					cout << "   │";
				}
			}
			if (i < 7)
			{
				cout << "\n ├ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┼ ─ ┤" << endl;
			}
		}
		cout << "\n └ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┴ ─ ┘" << endl << endl;
		if (turn == player)
		{
			cout << "이동: 방향키 / 종료: Q" << endl;
			while (1)
			{
				input = _getch();
				if (ChangePos(pos[player], input))
					break;
			}
			
			mybuf_r.buf[0] = input;
			WSASend(s_socket, &mybuf_r, 1, &recv_byte, 0, 0, 0);
		}
		else
		{
			cout << "상대방의 턴입니다." << endl;
			WSARecv(s_socket, &mybuf_r, 1, &recv_byte, &recv_flag, 0, 0);
			ChangePos(pos[(player + 1) % 2], mybuf_r.buf[0]);
		}

		turn++;
		turn %= 2;
	}
}