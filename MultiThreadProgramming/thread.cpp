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
	t1.join();	// main �Լ��� �����尡 ����� ������ ��⸦ �ϰ� ���Ḧ �ؾ��Ѵ�.
}