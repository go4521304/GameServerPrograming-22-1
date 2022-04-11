#include <iostream>
#include <thread>
#include <vector>
using namespace std;

constexpr int NUM_THREADS = 10;

void thread_func(int th_id)
{
	cout << "I am a Thread " << th_id << ".\n";
}

int main()
{
	vector<thread> threads;
	for (int i = 0; i < NUM_THREADS; ++i)
		threads.emplace_back(thread_func, i);

	cout << "Hello World from main\n";

	for (auto& th : threads)
		th.join();

}