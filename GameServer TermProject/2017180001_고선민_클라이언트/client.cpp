#define SFML_STATIC 1
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <string>
using namespace std;

#ifdef _DEBUG
#pragma comment (lib, "lib/sfml-graphics-s-d.lib")
#pragma comment (lib, "lib/sfml-window-s-d.lib")
#pragma comment (lib, "lib/sfml-system-s-d.lib")
#pragma comment (lib, "lib/sfml-network-s-d.lib")
#else
#pragma comment (lib, "lib/sfml-graphics-s.lib")
#pragma comment (lib, "lib/sfml-window-s.lib")
#pragma comment (lib, "lib/sfml-system-s.lib")
#pragma comment (lib, "lib/sfml-network-s.lib")
#endif
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "ws2_32.lib")

#include ".\protocol.h"
sf::TcpSocket socket;

constexpr auto SCREEN_WIDTH = 9;
constexpr auto SCREEN_HEIGHT = 9;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = TILE_WIDTH * SCREEN_WIDTH + 10;   // size of window
constexpr auto WINDOW_HEIGHT = TILE_WIDTH * SCREEN_WIDTH + 10;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow *g_window;
sf::Font g_font;

class OBJECT
{
private:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
	sf::Text m_chat;
	sf::Text m_ui_hp;
	sf::Text m_ui_lv;
	sf::Text m_ui_chat;
	chrono::system_clock::time_point m_mess_end_time;
public:
	int m_x, m_y;
	short m_race;
	short m_level;
	int	  m_exp;
	int   m_hp, m_hpmax;
	string m_msg;
	bool m_input;
	char m_chat_type;

	OBJECT(sf::Texture &t, int x, int y, int x2, int y2)
	{
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
		m_input = false;
		m_chat_type = 1;
	}
	OBJECT()
	{
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y)
	{
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw()
	{
		g_window->draw(m_sprite);
	}

	void move(int x, int y)
	{
		m_x = x;
		m_y = y;
	}
	void draw()
	{
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 8;
		float ry = (m_y - g_top_y) * 65.0f + 8;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		if (m_mess_end_time < chrono::system_clock::now())
		{
			m_name.setPosition(rx - 10, ry - 20);
			g_window->draw(m_name);
		}
		else
		{
			m_chat.setPosition(rx - 10, ry - 20);
			g_window->draw(m_chat);
		}

		m_ui_hp.setPosition(10, 0);
		g_window->draw(m_ui_hp);
		m_ui_lv.setPosition(10, 30);
		g_window->draw(m_ui_lv);

		if (m_input)
		{
			string ui_chat;
			if (m_chat_type == 1)
				ui_chat = "say ";
			else if (m_chat_type == 2)
				ui_chat = "tell ";
			else if (m_chat_type == 3)
				ui_chat = "shout ";

			ui_chat += m_msg;

			m_ui_chat.setString(ui_chat);
			m_ui_chat.setPosition(10, 60);
			g_window->draw(m_ui_chat);
		}
	}
	void set_name(const char str[])
	{
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	void set_chat(const char str[])
	{
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}

	void set_ui()
	{
		m_ui_hp.setFont(g_font);

		string ui_hp = {"HP " + to_string(m_hp)};
		ui_hp += "/";
		ui_hp += to_string(m_hpmax);

		m_ui_hp.setString(ui_hp);
		m_ui_hp.setFillColor(sf::Color(255, 0, 0));
		m_ui_hp.setStyle(sf::Text::Italic | sf::Text::Bold);

		m_ui_lv.setFont(g_font);
		m_ui_lv.setString("Lv " + to_string(m_level));
		m_ui_lv.setFillColor(sf::Color(0, 255, 0));
		m_ui_lv.setStyle(sf::Text::Italic | sf::Text::Bold);

		m_ui_chat.setFont(g_font);
		m_ui_chat.setString("");
		m_ui_chat.setFillColor(sf::Color(255, 255, 255));
		m_ui_chat.setStyle(sf::Text::Bold);
	}
};

OBJECT avatar;
unordered_map<int, OBJECT> players;
unordered_map<int, OBJECT> npcs;
//OBJECT players[MAX_USER];
// OBJECT npcs[NUM_NPC];

OBJECT white_tile;
OBJECT black_tile;

sf::Texture *board;
sf::Texture *pieces;

void client_finish()
{
	delete board;
	delete pieces;
}

void ProcessPacket(char *ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOGIN_OK:
	{
		SC_LOGIN_OK_PACKET *packet = reinterpret_cast<SC_LOGIN_OK_PACKET *>(ptr);
		g_myid = packet->id;
		avatar.m_race = packet->race;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		avatar.m_level = packet->level;
		avatar.m_exp = packet->exp;
		avatar.m_hp = packet->hp;
		avatar.m_hpmax = packet->hpmax;

		avatar.set_ui();

		g_left_x = packet->x - 4;
		g_top_y = packet->y - 4;
		avatar.show();
	} break;

	case SC_LOGIN_FAIL:
	{
		SC_LOGIN_FAIL_PACKET *packet = reinterpret_cast<SC_LOGIN_FAIL_PACKET *>(ptr);
		switch (packet->reason)
		{
		case 0:
			cout << "Invalid Name.\n";
			break;

		case 1:
			cout << "Name Already Playing.\n"; 
			break;

		case 2:
			cout << "Server Full.\n"; 
			break;

		default:
			break;
		}
	} break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET *my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET *>(ptr);
		int id = my_packet->id;

		if (id < MAX_USER)
		{
			if (0 != players.count(id))
			{
				cout << "Player " << id << " already exists.\n";
				break;
			}
			players[id] = OBJECT{*pieces, 64, 0, 64, 64};
			players[id].move(my_packet->x, my_packet->y);
			players[id].m_race = my_packet->race;
			players[id].m_level = my_packet->level;
			players[id].m_hp = my_packet->hp;
			players[id].m_hpmax = my_packet->hpmax;
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		else
		{
			if (0 != npcs.count(id))
			{
				cout << "NPC " << id << " already exists.\n";
				break;
			}
			npcs[id] = OBJECT{*pieces, 0, 0, 64, 64};
			npcs[id].move(my_packet->x, my_packet->y);
			npcs[id].m_race = my_packet->race;
			npcs[id].m_level = my_packet->level;
			npcs[id].m_hp = my_packet->hp;
			npcs[id].m_hpmax = my_packet->hpmax;
			npcs[id].set_name(my_packet->name);
			npcs[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET *my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid)
		{
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 4;
			g_top_y = my_packet->y - 4;
		}
		else if (other_id < MAX_USER)
		{
			if (0 == players.count(other_id))
			{
				cout << "Player " << other_id << " does not exists.\n";
				break;
			}
			players[other_id].move(my_packet->x, my_packet->y);
		}
		else
		{
			if (0 == npcs.count(other_id))
			{
				cout << "NPC " << other_id << " does not exists.\n";
				break;
			}
			npcs[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET *my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET *>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid)
		{
			avatar.hide();
		}
		else if (other_id < MAX_USER)
		{
			if (0 == players.count(other_id))
			{
				cout << "Player " << other_id << " does not exists.\n";
				break;
			}
			players[other_id].hide();
			players.erase(other_id);
		}
		else
		{
			if (0 == npcs.count(other_id))
			{
				cout << "NPC " << other_id << " does not exists.\n";
				break;
			}
			npcs[other_id].hide();
			npcs.erase(other_id);
		}
		break;
	}

	case SC_CHAT:
	{
		SC_CHAT_PACKET *packet = reinterpret_cast<SC_CHAT_PACKET *>(ptr);

		string msg;

		switch (packet->chat_type - 1)
		{
		case 0:
			msg = to_string(packet->id);
			break;

		case 1:
			msg = to_string(packet->id);
			break;

		case 2:
			msg = to_string(packet->id);
			break;

		case 3:
			msg = "공지";
			break;

		default:
			break;
		}
		msg += " ";
		msg += packet->mess;

		avatar.set_chat(msg.c_str());
	} break;

	case SC_STAT_CHANGE:
	{
		SC_STAT_CHANGE_PACKET *my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET *>(ptr);

		int id = my_packet->id;

		if (id < MAX_USER)
		{
			if (id == g_myid)
			{
				avatar.m_level = my_packet->level;
				avatar.m_exp = my_packet->exp;
				avatar.m_hp = my_packet->hp;
				avatar.m_hpmax = my_packet->hpmax;
				avatar.set_ui();
				break;
			}

			else if (0 == players.count(id))
			{
				cout << "Player " << id << " couldn't find.\n";
				break;
			}

			players[id].m_level = my_packet->level;
			players[id].m_exp = my_packet->exp;
			players[id].m_hp = my_packet->hp;
			players[id].m_hpmax = my_packet->hpmax;
		}
		else
		{
			if (0 == npcs.count(id))
			{
				cout << "NPC " << id << " couldn't find.\n";
				break;
			}
			npcs[id].m_level = my_packet->level;
			npcs[id].m_exp = my_packet->exp;
			npcs[id].m_hp = my_packet->hp;
			npcs[id].m_hpmax = my_packet->hpmax;
		}
	} break;

	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char *net_buf, size_t io_byte)
{
	char *ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte)
	{
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size)
		{
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else
		{
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		while (true);
	}
	if (recv_result == sf::Socket::Disconnected)
	{
		wcout << L"서버 접속 종료\n";
		client_finish();
		exit(0);

	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if ((tile_x >= W_WIDTH) || (tile_y >= W_HEIGHT)) continue;
			if (((tile_x / 3 + tile_y / 3) % 2) == 0)
			{
				white_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				black_tile.a_draw();
			}
		}
	avatar.draw();
	for (auto &pl : players) pl.second.draw();
	for (auto &pl : npcs) pl.second.draw();
}

void send_packet(void *packet)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(packet);
	size_t sent = 0;
	socket.send(packet, p[0], sent);
}

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	if (false == g_font.loadFromFile("cour.ttf"))
	{
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{*board, 5, 5, TILE_WIDTH, TILE_WIDTH};
	black_tile = OBJECT{*board, 69, 5, TILE_WIDTH, TILE_WIDTH};
	avatar = OBJECT{*pieces, 128, 0, 64, 64};
	avatar.move(4, 4);
}

int main()
{
	cout << "계정을 입력해주세요: ";
	string name;
	cin >> name;

	wcout.imbue(locale("korean"));
	sf::Socket::Status status = socket.connect("127.0.0.1", PORT_NUM);
	socket.setBlocking(false);


	if (status != sf::Socket::Done)
	{
		wcout << L"서버와 연결할 수 없습니다.\n";
		while (true);
	}

	client_initialize();

	CS_LOGIN_PACKET p;
	p.size = sizeof(CS_LOGIN_PACKET);
	p.type = CS_LOGIN;
	strcpy_s(p.name, name.c_str());
	avatar.set_name(name.c_str());
	send_packet(&p);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed)
			{
				int direction = -1;
				switch (event.key.code)
				{
				case sf::Keyboard::Left:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Up:
					direction = 0;
					break;
				case sf::Keyboard::Down:
					direction = 1;
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;

				case sf::Keyboard::Enter:
				{
					avatar.m_input = !avatar.m_input;
					if (avatar.m_input)
					{
						avatar.m_msg.clear();
					}
					else if (!avatar.m_msg.empty())
					{
						CS_CHAT_PACKET p;
						p.type = CS_CHAT;
						p.chat_type = avatar.m_chat_type;
						if (avatar.m_chat_type == 2)
						{
							try
							{
								auto it = find(avatar.m_msg.begin(), avatar.m_msg.end(), '/');
								string target = {avatar.m_msg.begin(), it}; 
								p.target_id = stoi(target);
								avatar.m_msg.erase(avatar.m_msg.begin(), it+1);
							}
							catch (const std::exception &)
							{
								p.target_id = -1;
								break;
							}
						}
						p.size = sizeof(p) - sizeof(p.mess) + avatar.m_msg.size() + 1;
						strcpy_s(p.mess, avatar.m_msg.c_str());
						send_packet(&p);
					}
				} break;

				case sf::Keyboard::Tab:
					if (avatar.m_input)
					{
						avatar.m_chat_type %= 3;
						avatar.m_chat_type++;
					} break;

				case sf::Keyboard::Space:
					if (avatar.m_input)
					{
						avatar.m_msg.push_back(' ');
					} break;

				case sf::Keyboard::A:
				case sf::Keyboard::B:
				case sf::Keyboard::C:
				case sf::Keyboard::D:
				case sf::Keyboard::E:
				case sf::Keyboard::F:
				case sf::Keyboard::G:
				case sf::Keyboard::H:
				case sf::Keyboard::I:
				case sf::Keyboard::J:
				case sf::Keyboard::K:
				case sf::Keyboard::L:
				case sf::Keyboard::M:
				case sf::Keyboard::N:
				case sf::Keyboard::O:
				case sf::Keyboard::P:
				case sf::Keyboard::Q:
				case sf::Keyboard::R:
				case sf::Keyboard::S:
				case sf::Keyboard::T:
				case sf::Keyboard::U:
				case sf::Keyboard::V:
				case sf::Keyboard::W:
				case sf::Keyboard::X:
				case sf::Keyboard::Y:
				case sf::Keyboard::Z:
					if (avatar.m_input)
						avatar.m_msg.push_back('A' + event.key.code);
					break;

				case sf::Keyboard::Slash:
				case sf::Keyboard::Divide:
					if (avatar.m_input)
						avatar.m_msg.push_back('/');
					break;

				case sf::Keyboard::Num0:
				case sf::Keyboard::Num1:
				case sf::Keyboard::Num2:
				case sf::Keyboard::Num3:
				case sf::Keyboard::Num4:
				case sf::Keyboard::Num5:
				case sf::Keyboard::Num6:
				case sf::Keyboard::Num7:
				case sf::Keyboard::Num8:
				case sf::Keyboard::Num9:
					if (avatar.m_input)
						avatar.m_msg.push_back('0' + (event.key.code - sf::Keyboard::Num0));
					break;

				case sf::Keyboard::Numpad0:
				case sf::Keyboard::Numpad1:
				case sf::Keyboard::Numpad2:
				case sf::Keyboard::Numpad3:
				case sf::Keyboard::Numpad4:
				case sf::Keyboard::Numpad5:
				case sf::Keyboard::Numpad6:
				case sf::Keyboard::Numpad7:
				case sf::Keyboard::Numpad8:
				case sf::Keyboard::Numpad9:
					if (avatar.m_input)
						avatar.m_msg.push_back('0' + (event.key.code - sf::Keyboard::Numpad0));
					break;

				case sf::Keyboard::Backspace:
					if (avatar.m_input && !avatar.m_msg.empty())
						avatar.m_msg.pop_back();
					break;

				case sf::Keyboard::LControl:
				{
					CS_ATTACK_PACKET packet;
					packet.size = sizeof(packet);
					packet.type = CS_ATTACK;
					send_packet(&packet);
				}

				default:
					break;
				}
				if (-1 != direction)
				{
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					send_packet(&p);
				}

			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}