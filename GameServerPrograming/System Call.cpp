#include <iostream>
#include <chrono>
#include <thread>

using namespace std;
using namespace chrono;

int main()
{
	volatile long long tmp = 0;
	auto start = high_resolution_clock::now();

	for (int j = 0; j < 10000000; ++j) {
		tmp += j;
		this_thread::yield();		// System Call
	}
	auto duration = high_resolution_clock::now() - start;

	cout << "Time " << duration_cast<milliseconds>(duration).count();
	cout << " msec\n";
	cout << "RESULT " << tmp << endl;
}
