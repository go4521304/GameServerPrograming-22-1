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

volatile int victim = 0;
volatile bool flags[2] = { false, false };

void p_lock(int th_id)
{
	int other = 1 - th_id;
	flags[th_id] = true;
	victim = th_id;
	atomic_thread_fence(memory_order_seq_cst);
	while ((flags[other] == true) && (victim == th_id));
}

void p_unlock(int th_id)
{
	flags[th_id] = false;
}

void thread_func(int th_id, int num_threads)
{
	for (int i = 0; i < LOOP / num_threads; ++i)
	{
		p_lock(th_id);
		sum = sum + 2;
		p_unlock(th_id);
	}
}


int main()
{
	for (int num = 2; num <= 2; num *= 2)
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