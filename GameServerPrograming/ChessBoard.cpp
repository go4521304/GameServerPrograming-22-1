#include <iostream>
#include <conio.h>
#include <Windows.h>

using namespace std;

constexpr int UP = 72;
constexpr int DOWN = 80;
constexpr int LEFT = 75;
constexpr int RIGHT = 77;

int main()
{
	pair<int, int> pos(0, 0);
	char input;

	while (1)
	{
		system("cls");
		cout << "   A   B   C   D   E   F   G   H" << endl;
		cout << " ┌ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┬ ─ ┐" << endl;
		for (int i = 0; i < 8; ++i)
		{
			cout << i+1 << "│";
			for (int j = 0; j < 8; ++j)
			{
				if (pos.first == i && pos.second == j)
				{
					cout << " ★│";
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
		cout << "이동: 방향키 / 종료: Q" << endl;
		input = _getch();

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
			break;
		}
	}
}