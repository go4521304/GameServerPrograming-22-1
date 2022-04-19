#include <iostream>
#include <thread>

using namespace std;

int g_data = 0;
bool ready = false;

void Reciver()
{
	while (false == ready);
	int my_data = g_data;
	cout << "Result = " << my_data << endl;
}


void Sender()
{
	g_data = 999;
	ready = true;
}

int main()
{
	thread t1{ Reciver };
	thread t2{ Sender };

	t1.join();
	t2.join();
}