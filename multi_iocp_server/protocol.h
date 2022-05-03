#pragma once
constexpr int NAME_SIZE = 10;
constexpr int PORT_NUM = 6000;
constexpr int BUF_SIZE = 200;

constexpr int W_WIDTH = 400;
constexpr int W_HEIGHT = 400;

constexpr int MAX_USER = 2000;

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;

constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_ADD_PLAYER = 3;
constexpr char SC_REMOVE_PLAYER = 4;
constexpr char SC_MOVE_PLAYER = 5;


#pragma pack (push, 1)
struct CS_LOGIN_PACKET
{
	unsigned char	size;
	char			type;
	char			name[NAME_SIZE];
};

struct CS_MOVE_PACKET
{
	unsigned char	size;
	char			type;
	char			direction;	// 0: UP, 1: DOWN, 2: LEFT, 3: RIGHT
	unsigned int client_time;
};


struct SC_LOGIN_INFO_PACKET
{
	unsigned char	size;
	char			type;
	short			id;
	short			x, y;
};

struct SC_ADD_PLAYER_PACKET
{
	unsigned char	size;
	char			type;
	short			id;
	char			name[NAME_SIZE];
	short			x, y;
};

struct SC_REMOVE_PLAYER_PACKET
{
	unsigned char	size;
	char			type;
	short			id;
};

struct SC_MOVE_PLAYER_PACKET
{
	unsigned char	size;
	char			type;
	short			id;
	short			x, y;
	unsigned int client_time;
};

#pragma pack (pop)