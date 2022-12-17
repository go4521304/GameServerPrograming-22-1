#include <iostream>
#include <array>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <queue>
#include <chrono>
#include <string>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include "protocol.h"

#define UNICODE  
#include <sqlext.h>  

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

extern "C" {
#include "include/lua.h"
#include "include/lauxlib.h"
#include "include/lualib.h"
}

#pragma comment (lib, "lua54.lib")



using namespace std;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_LOGIN_TRY, OP_LOGIN_FAIL, OP_LOGIN_SUCCESS, OP_FAI, OP_PLAYER_MOVE, OP_HP_HEAL, OP_AUTOSAVE, OP_HP_CHANGE };
enum SESSION_STATE { ST_FREE, ST_ACCEPTED, ST_INGAME };
enum EVENT_TYPE {EVENT_HP_HEAL, EVENT_AUTOSAVE};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class OVER_EXP
{
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	int target_id;
	int info;

	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char *packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, packet[0]);
	}
};

class SESSION
{
	OVER_EXP _recv_over;

public:
	mutex _sl;
	SESSION_STATE _s_state;
	int _id;
	SOCKET _socket;
	short _x, _y;
	char _name[NAME_SIZE];
	short _race;
	short _level;
	int	  _exp;
	int   _hp, _hpmax;
	unordered_set<int> view_list;
	mutex vl;
	mutex vm_l;

	lua_State *L;

	chrono::system_clock::time_point next_move_time;
	int _prev_remain;

public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		_x = rand() % W_WIDTH;
		_y = rand() % W_HEIGHT;
		_name[0] = 0;
		_s_state = ST_FREE;
		_prev_remain = 0;
		next_move_time = chrono::system_clock::now() + chrono::seconds(1);
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over,
			0);
	}

	void do_send(void *packet)
	{
		OVER_EXP *sdata = new OVER_EXP{reinterpret_cast<char *>(packet)};
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}

	void send_login_fail(int reason);
	void send_login_ok();
	void send_add_object(int c_id);
	void send_remove_object(int c_id);
	void send_move_object(int c_id, int client_time);
	void send_chat(int c_id, char chat_type, const char *mess);
	void send_stat_change(int c_id);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SOCKET g_s_socket;
HANDLE g_h_iocp;

array<SESSION, MAX_USER + NUM_NPC> clients;

struct event_type
{
	int object_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;
	int target_id;

	constexpr bool operator < (const event_type &_left) const
	{
		return (wakeup_time > _left.wakeup_time);
	}
};

priority_queue<event_type> timer_queue;
mutex timer_lock;

void disconnect(int c_id);
void do_worker();
int get_new_client_id();
void process_packet(int c_id, char *packet);
int distance(int a, int b);
void do_timer();
void process_timer(event_type e);
void add_timer(int object_id, int target_id, EVENT_TYPE type, int time);
void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);
void login_user(int c_id);
void update_user(int c_id);

void move_npc(int npc_id)
{
	short x = clients[npc_id]._x;
	short y = clients[npc_id]._y;
	unordered_set<int> old_vl;
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (clients[i]._s_state != ST_INGAME) continue;
		if (distance(npc_id, i) <= RANGE) old_vl.insert(i);
	}
	switch (rand() % 4)
	{
	case 0: if (y > 0) y--; break;
	case 1: if (y < W_HEIGHT - 1) y++; break;
	case 2: if (x > 0) x--; break;
	case 3: if (x < W_WIDTH - 1) x++; break;
	}

	volatile int i = 0;
	volatile int sum = 0;
	for (int i = 0; i < 10000; ++i)
		sum += i;

	clients[npc_id]._x = x;
	clients[npc_id]._y = y;

	unordered_set<int> new_vl;
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (clients[i]._s_state != ST_INGAME) continue;
		if (distance(npc_id, i) <= RANGE) new_vl.insert(i);
	}

	for (auto p_id : new_vl)
	{
		clients[p_id].vl.lock();
		if (0 == clients[p_id].view_list.count(npc_id))
		{
			clients[p_id].view_list.insert(npc_id);
			clients[p_id].vl.unlock();
			clients[p_id].send_add_object(npc_id);
		}
		else
		{
			clients[p_id].vl.unlock();
			clients[p_id].send_move_object(npc_id, 0);
		}
	}
	for (auto p_id : old_vl)
	{
		if (0 == new_vl.count(p_id))
		{
			clients[p_id].vl.lock();
			if (clients[p_id].view_list.count(npc_id) == 1)
			{
				clients[p_id].view_list.erase(npc_id);
				clients[p_id].vl.unlock();
				clients[p_id].send_remove_object(npc_id);
			}
			else
				clients[p_id].vl.unlock();
		}
	}
}


int API_get_x(lua_State *L)
{
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);

	int x = clients[obj_id]._x;

	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State *L)
{
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);

	int y = clients[obj_id]._y;

	lua_pushnumber(L, y);
	return 1;
}

void initialize_npc()
{
	for (int i = 0; i < NUM_NPC + MAX_USER; ++i)
		clients[i]._id = i;
	cout << "NPC initialize Begin.\n";
	for (int i = 0; i < NUM_NPC; ++i)
	{
		int npc_id = i + MAX_USER;
		clients[npc_id]._s_state = ST_INGAME;
		sprintf_s(clients[npc_id]._name, "M-%d", npc_id);
		lua_State *L = luaL_newstate();
		clients[npc_id].L = L;

		luaL_openlibs(L);
		luaL_loadfile(L, "hello.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_object_id");
		lua_pushnumber(L, npc_id);
		lua_pcall(L, 1, 0, 0);

		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
	cout << "NPC Initialization complete.\n";
}

void do_ai_ver_heat_beat()
{
	for (;;)
	{
		auto start_t = chrono::system_clock::now();
		for (int i = 0; i < NUM_NPC; ++i)
		{
			int npc_id = i + MAX_USER;
			move_npc(npc_id);
			lua_getglobal(clients[npc_id].L, "event_npc_check");
			lua_pcall(clients[npc_id].L, 0, 0, 0);
		}
		auto end_t = chrono::system_clock::now();
		auto ai_t = end_t - start_t;
		cout << "AI time : " << chrono::duration_cast<chrono::milliseconds>(ai_t).count();
		cout << "ms\n";
		this_thread::sleep_until(start_t + chrono::seconds(1));
	}
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr *>(&server_addr),
		sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	int client_id = 0;

	g_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), g_h_iocp, 9999,
		0);
	SOCKET c_socket =
		WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	OVER_EXP a_over;
	a_over._comp_type = OP_ACCEPT;
	a_over._wsabuf.buf = reinterpret_cast<CHAR *>(c_socket);
	AcceptEx(g_s_socket, c_socket, a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &a_over._over);
	
	vector<thread> worker_thread;
	for (int i = 0; i < 4; ++i) worker_thread.emplace_back(do_worker);

	initialize_npc();

	thread timer_thread{do_timer};
	thread ai_thread{do_ai_ver_heat_beat};
	ai_thread.join();
	for (auto &th : worker_thread) th.join();
	timer_thread.join();

	closesocket(g_s_socket);
	WSACleanup();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void do_worker()
{
	while (true)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED *over = nullptr;
		BOOL ret =
			GetQueuedCompletionStatus(g_h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP *ex_over = reinterpret_cast<OVER_EXP *>(over);
		int client_id = static_cast<int>(key);
		if (FALSE == ret)
		{
			if (ex_over->_comp_type == OP_ACCEPT)
				cout << "Accept Error";
			else
			{
				cout << "GQCS Error on client[" << key << "]\n";
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
				clients[client_id]._x = 0;
				clients[client_id]._y = 0;
				clients[client_id]._id = client_id;
				clients[client_id]._name[0] = 0;
				clients[client_id]._prev_remain = 0;
				clients[client_id]._socket = c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), g_h_iocp,
					client_id, 0);

				clients[client_id].do_recv();
				c_socket =
					WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}

			// Server Full
			else
			{
				cout << "Max User Exceeded. \n";
				auto tmp_ex_over = new OVER_EXP;
				tmp_ex_over->_comp_type = OP_LOGIN_FAIL;
				tmp_ex_over->target_id = client_id;
				tmp_ex_over->info = 2;
				PostQueuedCompletionStatus(g_h_iocp, 1, client_id, &tmp_ex_over->_over);
			}
			ZeroMemory(&ex_over->_over, sizeof(ex_over->_over));
			ex_over->_wsabuf.buf = reinterpret_cast<CHAR *>(c_socket);
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, c_socket, ex_over->_send_buf, 0, addr_size + 16, addr_size + 16, 0, &ex_over->_over);
		} break;

		case OP_RECV:
		{
			if (num_bytes == 0) disconnect(client_id);

			int data_size = num_bytes + clients[client_id]._prev_remain;
			char *packet = ex_over->_send_buf;

			while (data_size > 0 && data_size <= packet[0])
			{
				process_packet(client_id, packet);

				int packet_size = packet[0];
				packet = packet + packet_size;
				data_size -= packet_size;
			}

			clients[client_id]._prev_remain = data_size;
			if (data_size > 0)
			{
				memcpy(ex_over->_send_buf, packet, data_size);
			}
			clients[client_id].do_recv();

		} break;

		case OP_SEND:
		{
			if (0 == num_bytes) disconnect(client_id);
			delete ex_over;
		} break;

		// ******************************* LOGIN ******************************* //
		case OP_LOGIN_TRY:
		{
			login_user(client_id);
		} break;

		case OP_LOGIN_FAIL:
		{
			clients[client_id].send_login_fail(ex_over->info);
			clients[client_id]._sl.lock();
			if (clients[client_id]._s_state == ST_FREE)
			{
				clients[client_id]._sl.unlock();
				continue;
			}
			//closesocket(clients[client_id]._socket);
			clients[client_id]._s_state = ST_FREE;
			clients[client_id]._sl.unlock();
		} break;

		case OP_LOGIN_SUCCESS:
		{
			clients[client_id].send_login_ok();
			add_timer(client_id, client_id, EVENT_HP_HEAL, 5);
			add_timer(client_id, client_id, EVENT_AUTOSAVE, 60);

			for (int i = 0; i < MAX_USER; ++i)
			{
				auto &pl = clients[i];
				if (pl._id == client_id) continue;
				pl._sl.lock();
				if (ST_INGAME != pl._s_state)
				{
					pl._sl.unlock();
					continue;
				}
				if (RANGE >= distance(client_id, pl._id))
				{
					pl.vl.lock();
					pl.view_list.insert(client_id);
					pl.vl.unlock();
					pl.send_add_object(client_id);
				}
				pl._sl.unlock();
			}
			for (auto &pl : clients)
			{
				if (pl._id == client_id) continue;
				lock_guard<mutex> aa{pl._sl};
				if (ST_INGAME != pl._s_state) continue;

				if (RANGE >= distance(pl._id, client_id))
				{
					clients[client_id].vl.lock();
					clients[client_id].view_list.insert(pl._id);
					clients[client_id].vl.unlock();
					clients[client_id].send_add_object(pl._id);
				}
			}
		} break;

		case OP_HP_HEAL:
		{
			clients[client_id]._sl.lock();
			if (clients[client_id]._s_state != ST_INGAME)
			{
				clients[client_id]._sl.unlock();
				continue;
			}
			
			clients[client_id]._hp += ex_over->info;
			if (clients[client_id]._hp > clients[client_id]._hpmax)
				clients[client_id]._hp = clients[client_id]._hpmax;
			clients[client_id]._sl.unlock();


			add_timer(client_id, client_id, EVENT_HP_HEAL, 5);
		} break;

		case OP_AUTOSAVE:
		{
			if (clients[client_id]._s_state != ST_INGAME) continue;

			update_user(client_id);
			add_timer(client_id, client_id, EVENT_AUTOSAVE, 60);
		} break;

		case OP_HP_CHANGE:
		{
			int victim = ex_over->target_id;

			clients[victim]._sl.lock();
			if (clients[client_id]._s_state != ST_INGAME && clients[victim]._s_state != ST_INGAME)
			{
				clients[victim]._sl.unlock();
				continue;
			}
			clients[victim]._hp += ex_over->info;

			if (clients[victim]._hp > clients[victim]._hpmax)
				clients[victim]._hp = clients[victim]._hpmax;
			clients[victim]._sl.unlock();


			if (clients[client_id]._hp <= 0)
			{
				clients[client_id].vl.lock();
				unordered_set<int> old_vl = {clients[client_id].view_list.begin(), clients[client_id].view_list.end()};
				clients[client_id].vl.unlock();

				for (const auto &i : old_vl)
				{
					clients[victim].send_remove_object(i);
				}

				clients[victim]._sl.lock();
				clients[victim]._x = 0;
				clients[victim]._y = 0;
				clients[victim]._hp = clients[victim]._hpmax;
				clients[victim]._exp /= 2;
				clients[victim]._sl.unlock();

				clients[victim].send_move_object(victim, 0);
				clients[victim].send_stat_change(victim);

				for (int i = 0; i < MAX_USER; ++i)
				{
					auto &pl = clients[i];
					if (pl._id == client_id) continue;
					pl._sl.lock();
					if (ST_INGAME != pl._s_state)
					{
						pl._sl.unlock();
						continue;
					}
					if (RANGE >= distance(clients[victim]._id, pl._id))
					{
						pl.vl.lock();
						pl.view_list.insert(clients[victim]._id);
						pl.vl.unlock();
						pl.send_add_object(clients[victim]._id);
					}
					pl._sl.unlock();
				}

				for (auto &pl : clients)
				{
					if (pl._id == client_id) continue;
					lock_guard<mutex> aa{pl._sl};
					if (ST_INGAME != pl._s_state) continue;

					if (RANGE >= distance(pl._id, victim))
					{
						clients[victim].vl.lock();
						clients[victim].view_list.insert(pl._id);
						clients[victim].vl.unlock();
						clients[victim].send_add_object(pl._id);
					}
				}
			}
			else
			{
				clients[victim].send_stat_change(victim);
			}
		} break;

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

	update_user(c_id);

	for (auto &pl : clients)
	{
		if (pl._id == c_id) continue;

		pl._sl.lock();
		if (pl._s_state != ST_INGAME)
		{
			pl._sl.unlock();
			continue;
		}
		SC_REMOVE_OBJECT_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_OBJECT;
		pl.do_send(&p);
		pl._sl.unlock();
	}
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

void process_packet(int c_id, char *packet)
{
	switch (packet[1])
	{
	case CS_LOGIN:
	{
		CS_LOGIN_PACKET *p = reinterpret_cast<CS_LOGIN_PACKET *>(packet);
		auto ex_over = new OVER_EXP;

		clients[c_id]._sl.lock();
		if (clients[c_id]._s_state == ST_FREE)
		{
			clients[c_id]._sl.unlock();
			break;
		}
		if (clients[c_id]._s_state == ST_INGAME)
		{
			clients[c_id]._sl.unlock();
			ex_over->_comp_type = OP_LOGIN_FAIL;
			ex_over->target_id = c_id;
			ex_over->info = 1;
			PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &ex_over->_over);
			break;
		}

		strcpy_s(clients[c_id]._name, p->name);
		clients[c_id]._sl.unlock();

		ex_over->_comp_type = OP_LOGIN_TRY;
		ex_over->target_id = c_id;
		PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &ex_over->_over);
	} break;

	case CS_LOGIN_STRESS:
	{
		CS_LOGIN_STRESS_PACKET *p = reinterpret_cast<CS_LOGIN_STRESS_PACKET *>(packet);
		auto ex_over = new OVER_EXP;

		clients[c_id]._sl.lock();
		if (clients[c_id]._s_state == ST_FREE)
		{
			clients[c_id]._sl.unlock();
			break;
		}
		if (clients[c_id]._s_state == ST_INGAME)
		{
			clients[c_id]._sl.unlock();
			ex_over->_comp_type = OP_LOGIN_FAIL;
			ex_over->target_id = c_id;
			ex_over->info = 1;
			PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &ex_over->_over);
			break;
		}

		strcpy_s(clients[c_id]._name, p->name);
		clients[c_id]._sl.unlock();

		clients[c_id]._x = rand() % W_WIDTH;
		clients[c_id]._y = rand() % W_HEIGHT;
		ex_over->_comp_type = OP_LOGIN_SUCCESS;
		ex_over->target_id = c_id;
		PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &ex_over->_over);
	}

	case CS_MOVE:
	{
		CS_MOVE_PACKET *p = reinterpret_cast<CS_MOVE_PACKET *>(packet);
		short x = clients[c_id]._x;
		short y = clients[c_id]._y;

		clients[c_id].vl.lock();
		unordered_set<int> old_vl = {clients[c_id].view_list.begin(), clients[c_id].view_list.end()};
		clients[c_id].vl.unlock();


		switch (p->direction)
		{
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		clients[c_id]._x = x;
		clients[c_id]._y = y;
		clients[c_id].send_move_object(c_id, p->client_time);

		unordered_set<int> new_vl;
		for (int i = 0; i < MAX_USER; ++i)
		{
			if (clients[i]._s_state != ST_INGAME) continue;
			if (i == c_id) continue;
			if (distance(c_id, i) <= RANGE) new_vl.insert(i);
		}

		for (auto p_id : new_vl)
		{
			if (old_vl.count(p_id) == 0)
			{
				clients[p_id].send_add_object(c_id);
				clients[p_id].vl.lock();
				clients[p_id].view_list.insert(c_id);
				clients[p_id].vl.unlock();
			}
			else
			{
				clients[p_id].send_move_object(c_id, p->client_time);
			}
		}

		for (auto p_id : old_vl)
		{
			if (new_vl.count(p_id) == 0)
			{
				clients[p_id].send_remove_object(c_id);
				clients[p_id].vl.lock();
				clients[p_id].view_list.erase(c_id);
				clients[p_id].vl.unlock();
			}
		}

		clients[c_id].vl.lock();
		clients[c_id].view_list.clear();
		for (const auto &i : new_vl)
		{
			clients[c_id].view_list.insert(i);
		}
		clients[c_id].vl.unlock();
		

		//for (auto p_id : new_vl)
		//{
		//	clients[p_id].vl.lock();
		//	if (0 == new_vl.count(c_id))
		//	{
		//		clients[p_id].view_list.insert(c_id);
		//		clients[p_id].vl.unlock();
		//		clients[p_id].send_add_object(c_id);
		//	}
		//	else
		//	{
		//		clients[p_id].vl.unlock();
		//		clients[p_id].send_move_object(c_id, p->client_time);
		//	}
		//}

		//for (auto p_id : old_vl)
		//{
		//	if (0 == new_vl.count(p_id))
		//	{
		//		clients[p_id].vl.lock();
		//		if (clients[p_id].view_list.count(c_id) == 1)
		//		{
		//			clients[p_id].view_list.erase(c_id);
		//			clients[p_id].vl.unlock();
		//			clients[p_id].send_remove_object(c_id);
		//		}
		//		else
		//			clients[p_id].vl.unlock();
		//	}
		//}

		for (int i = 0; i < NUM_NPC; ++i)
		{
			int npc_id = MAX_USER + i;
			if (distance(npc_id, c_id) < RANGE)
			{
				auto ex_over = new OVER_EXP;
				ex_over->_comp_type = OP_PLAYER_MOVE;
				ex_over->target_id = c_id;
				PostQueuedCompletionStatus(g_h_iocp, 1, npc_id, &ex_over->_over);
			}
		}
		break;
	} break;

	case CS_ATTACK:
	{
		CS_ATTACK_PACKET *p = reinterpret_cast<CS_ATTACK_PACKET *>(packet);
		auto ex_over = new OVER_EXP;
		ex_over->_comp_type = OP_HP_CHANGE;
		ex_over->target_id = c_id;
		ex_over->info = -10;
		PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &ex_over->_over);

	} break;

	case CS_CHAT:
	{
		CS_CHAT_PACKET *p = reinterpret_cast<CS_CHAT_PACKET *>(packet);

		if (p->chat_type == 1)	// say
		{
			for (auto &p_id : clients[c_id].view_list)
			{
				clients[p_id].send_chat(c_id, p->chat_type, p->mess);
			}
		}
		else if (p->chat_type == 2)	// tell
		{
			clients[p->target_id].send_chat(c_id, p->chat_type, p->mess);
		}
		else if (p->chat_type == 3)	// shout
		{
			for (int i = 0; i < MAX_USER; ++i)
			{
				if (clients[i]._s_state != ST_INGAME) continue;
				if (i == c_id) continue;
				clients[i].send_chat(c_id, p->chat_type, p->mess);
			}
		}
	} break;

	default:
		break;
	}
}

int distance(int a, int b)
{
	return abs(clients[a]._x - clients[b]._x) + abs(clients[a]._y - clients[b]._y);
}

void do_timer()
{
	while (true)
	{
		auto start_t = chrono::system_clock::now();
		while (!timer_queue.empty())
		{
			event_type e = timer_queue.top();
			if (e.wakeup_time > start_t)
				break;

			timer_lock.lock();
			timer_queue.pop();
			timer_lock.unlock();

			process_timer(e);
		}
		this_thread::sleep_until(start_t + chrono::milliseconds(100));
	}
}

void process_timer(event_type e)
{
	switch (e.event_id)
	{
	case EVENT_HP_HEAL:
	{
		auto ex_over = new OVER_EXP;
		ex_over->_comp_type = OP_HP_HEAL;
		ex_over->target_id = e.target_id;
		ex_over->info = clients[e.object_id]._hpmax * 0.1;
		PostQueuedCompletionStatus(g_h_iocp, 1, e.object_id, &ex_over->_over);
		break;
	} break;

	case EVENT_AUTOSAVE:
	{
		auto ex_over = new OVER_EXP;
		ex_over->_comp_type = OP_AUTOSAVE;
		ex_over->target_id = e.target_id;
		PostQueuedCompletionStatus(g_h_iocp, 1, e.object_id, &ex_over->_over);
	} break;

	default:
		break;
	}
}

void add_timer(int object_id, int target_id, EVENT_TYPE type, int time)
{
	auto wake_time = chrono::system_clock::now() + chrono::seconds(time);

	event_type e;
	e.object_id = object_id;
	e.target_id = target_id;
	e.event_id = type;
	e.wakeup_time = wake_time;

	timer_lock.lock();
	timer_queue.push(e);
	timer_lock.unlock();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE)
	{
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT *)NULL) == SQL_SUCCESS)
	{
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5))
		{
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

void login_user(int c_id)
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;
	SQLWCHAR name[NAME_SIZE]{};
	SQLINTEGER exp = 0, lv = 0, hp = 0, race = 0, posX = 0, posY = 0;
	SQLLEN nameLen = 0, expLen = 0, lvLen = 0, raceLen = 0, hpLen = 0, posXLen = 0, posYLen = 0;

	setlocale(LC_ALL, "Korean");

	bool login = false;

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER *)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR *)L"2017180001_고선민_GameServer22", SQL_NTS, (SQLWCHAR *)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

					clients[c_id]._sl.lock();
					wstring user_id(clients[c_id]._name, &clients[c_id]._name[NAME_SIZE]);
					std::wstring s = (L"EXEC search_user " + user_id);

					retcode = SQLExecDirect(hstmt, (SQLWCHAR *)s.c_str(), SQL_NTS);
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{

						// Bind columns 1, 2, and 3  
						retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, name, NAME_SIZE, &nameLen);
						retcode = SQLBindCol(hstmt, 2, SQL_C_SLONG, &exp, NUM_LEN, &expLen);
						retcode = SQLBindCol(hstmt, 3, SQL_C_SLONG, &lv, NUM_LEN, &lvLen);
						retcode = SQLBindCol(hstmt, 4, SQL_C_SLONG, &hp, NUM_LEN, &hpLen);
						retcode = SQLBindCol(hstmt, 5, SQL_C_SLONG, &race, NUM_LEN, &raceLen);
						retcode = SQLBindCol(hstmt, 6, SQL_C_SLONG, &posX, NUM_LEN, &posXLen);
						retcode = SQLBindCol(hstmt, 7, SQL_C_SLONG, &posY, NUM_LEN, &posYLen);

						// Fetch and print each row of data. On an error, display a message and exit.  
						for (int i = 0; ; i++)
						{
							retcode = SQLFetch(hstmt);
							if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
							{
								HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);

							}

							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
							{
								clients[c_id]._x = posX;
								clients[c_id]._y = posY;
								size_t cnt;
								wcstombs_s(&cnt, clients[c_id]._name, nameLen, name, NAME_SIZE);
								clients[c_id]._race = race;
								clients[c_id]._level = lv;
								clients[c_id]._exp = exp;
								clients[c_id]._hp = hp;
								clients[c_id]._hpmax = lv * 100;
								clients[c_id]._s_state = ST_INGAME;

								login = true;
							}
							else
							{
								break;
							}
						}
					}
					else
					{
						HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
					}

					// unlock
					clients[c_id]._sl.unlock();

					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{
						SQLCancel(hstmt);
						SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
					}

					SQLDisconnect(hdbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}

	auto tmp_ex_over = new OVER_EXP;

	if (login)
	{
		tmp_ex_over->_comp_type = OP_LOGIN_SUCCESS;
		tmp_ex_over->target_id = c_id;
		PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &tmp_ex_over->_over);

	}
	else
	{
		tmp_ex_over->_comp_type = OP_LOGIN_FAIL;
		tmp_ex_over->target_id = c_id;
		tmp_ex_over->info = 0;
		PostQueuedCompletionStatus(g_h_iocp, 1, c_id, &tmp_ex_over->_over);
	}
}

void update_user(int c_id)
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;

	setlocale(LC_ALL, "Korean");


	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER *)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR *)L"2017180001_고선민_GameServer22", SQL_NTS, (SQLWCHAR *)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				{
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

					wstring user_name(clients[c_id]._name, &clients[c_id]._name[strlen(clients[c_id]._name)]);

					std::wstring s = (L"EXEC update_user " + user_name + L", " + to_wstring(clients[c_id]._exp) + L", " + to_wstring(clients[c_id]._level)
						+ L", " + to_wstring(clients[c_id]._hp) +L", " + to_wstring(clients[c_id]._x) + L", " + to_wstring(clients[c_id]._y));

					retcode = SQLExecDirect(hstmt, (SQLWCHAR *)s.c_str(), SQL_NTS);

					// Process data  
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
					{
						SQLCancel(hstmt);
						SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

						cout << c_id << " client update\n";
					}
					else
					{
						HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
					}

					SQLDisconnect(hdbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SESSION::send_login_fail(int reason)
{
	SC_LOGIN_FAIL_PACKET p;
	p.size = sizeof(p);
	p.type = SC_LOGIN_FAIL;
	p.reason = reason;
	do_send(&p);
}

void SESSION::send_login_ok()
{
	SC_LOGIN_OK_PACKET p;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.id = _id;
	p.race = _race;
	p.x = _x;
	p.y = _y;
	p.level = _level;
	p.exp = _exp;
	p.hp = _hp;
	p.hpmax = _hpmax;
	do_send(&p);
}

void SESSION::send_add_object(int c_id)
{
	SC_ADD_OBJECT_PACKET p;
	p.size = sizeof(p);
	p.type = SC_ADD_OBJECT;
	p.id = c_id;
	p.x = clients[c_id]._x;
	p.y = clients[c_id]._y;
	p.race = clients[c_id]._race;
	strcpy_s(p.name, clients[c_id]._name);
	p.level = clients[c_id]._level;
	p.hp = clients[c_id]._hp;
	p.hpmax = clients[c_id]._hpmax;
	do_send(&p);
}

void SESSION::send_remove_object(int c_id)
{
	SC_REMOVE_OBJECT_PACKET p;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	p.id = c_id;
	do_send(&p);
}

void SESSION::send_move_object(int c_id, int client_time)
{
	SC_MOVE_OBJECT_PACKET p;
	p.size = sizeof(p);
	p.type = SC_MOVE_OBJECT;
	p.id = c_id;
	p.x = clients[c_id]._x;
	p.y = clients[c_id]._y;
	p.client_time = client_time;
	do_send(&p);
}

void SESSION::send_chat(int c_id, char chat_type, const char *mess)
{
	SC_CHAT_PACKET p;
	p.size = sizeof(SC_CHAT_PACKET) - sizeof(p.mess) + strlen(mess) + 1;
	p.type = SC_CHAT;
	p.id = c_id;
	p.chat_type = chat_type;
	strcpy_s(p.mess, mess);
	do_send(&p);
}

void SESSION::send_stat_change(int c_id)
{
	SC_STAT_CHANGE_PACKET p;
	p.size = sizeof(p);
	p.type = SC_STAT_CHANGE;
	p.id = c_id;
	p.level = clients[c_id]._level;
	p.exp = clients[c_id]._exp;
	p.hp = clients[c_id]._hp;
	p.hpmax = clients[c_id]._hpmax;
	do_send(&p);
}