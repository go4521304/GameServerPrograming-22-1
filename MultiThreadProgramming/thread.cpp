#include <iostream>
#include <thread>
using namespace std;

void thread_func()
{
	cout << "I am a Thread.\n";
}

int main()
{
	thread t1{ thread_func };
	cout << "Hello World\n";
	t1.join();	// main 함수는 스레드가 종료될 때까지 대기를 하고 종료를 해야한다.
}