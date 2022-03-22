#include <iostream>
#include <chrono>
#include <thread>
using namespace std;
using namespace chrono;

constexpr int CACHE_LINE_SIZE = 32;

int main()
{
	for (int i = 0; i < 20; ++i)
	{
		const int size = 1024 << i;
		long long* a = (long long*)malloc(size);
		unsigned int index = 0;
		long long tmp = 0;
		const int num_data = size / 8;
		auto start = high_resolution_clock::now();

		for (int j = 0; j < 100000000; ++j)
		{
			tmp += a[index % num_data];
			index += CACHE_LINE_SIZE * 11;
		}

		auto duration = high_resolution_clock::now() - start;

		cout << "Time " << duration_cast<milliseconds>(duration).count();
		cout << " msec, Size " << size / (1024 * 1024) << " Result = " << tmp << endl;
	}
}
