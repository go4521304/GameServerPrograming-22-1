#include <iostream>
#include <thread>

using namespace std;

volatile bool done = false;
volatile int a = 0;
volatile int* b = &a;
int error;

volatile int g_data = 0;
volatile bool ready = false;

void t1()
{
	for (int i = 0; i < 2500000; ++i)
		*b = -(1 + *b);
	done = true;
}


void t2()
{
	while (false == done)
	{
		int v = *b;
		if (v != 0 && v != -1)
			error++;
	}
}

int main()
{
	thread th1{ t1 };
	thread th2{ t2 };

	th1.join();
	th2.join();
	printf("Number of Error = %d", error);
}