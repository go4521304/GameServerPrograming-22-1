#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
using namespace std;
using namespace std::chrono;

constexpr int NUM_THREADS = 4;
constexpr int LOOP = 500000000;

volatile int sum;

struct NUM {
	alignas(64) volatile int sum;
};

volatile NUM t_sum[16];
mutex my_lock;

void thread_func(int th_id, int num_threads)
{
	volatile int local_sum = 0;
	for (int i = 0; i < LOOP / num_threads; ++i)
	{
		t_sum[th_id].sum = t_sum[th_id].sum + 2;
	}
	my_lock.lock();
	sum = sum + t_sum[th_id].sum;
	my_lock.unlock();
}


int main()
{
	for (int num = 1; num <= 16; num *= 2)
	{
		sum = 0;
		for (auto& s : t_sum) s.sum = 0;
		vector<thread> threads;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num; ++i)
			threads.emplace_back(thread_func, i, num);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		cout << num << " Threads, ";
		cout << "Sum = " << sum << " Multi Thread Exec Time = ";
		cout << duration_cast<milliseconds>(exec_t).count() << "ms.\n";
	}
}