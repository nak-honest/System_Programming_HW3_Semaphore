#include "TestCase1.h"

void* Tc1ThreadProc(void* param)
{
	thread_t tid = 0;
	int count = 0;
	int i;

	tid = thread_self();

	for(int i=0;i<5;i++){
		sleep(2);
		printf("Tc1ThreadProc: my thread id (%d), arg is (%d), thread_cputime is (%d), count is (%d)\n", (int)tid, *((int*)param),thread_cputime(),count);
		count++;
	}
	thread_exit();
	return NULL;
}

void TestCase1(void)
{
	thread_t tid[TC1_THREAD_NUM];

	int i = 0, i1 = 1, i2 = 2, i3 = 3, i4 = 4, i5 = 5;

	thread_create(&tid[0], NULL, (void*)Tc1ThreadProc,(void*) &i1);	
	thread_create(&tid[1], NULL, (void*)Tc1ThreadProc,(void*) &i2);	
	thread_create(&tid[2], NULL, (void*)Tc1ThreadProc,(void*) &i3);	
	thread_create(&tid[3], NULL, (void*)Tc1ThreadProc,(void*) &i4);	
	thread_create(&tid[4], NULL, (void*)Tc1ThreadProc,(void*) &i5);

	for(i=0;i<TC1_THREAD_NUM;i++)
	{
		thread_join(tid[i]);

		printf("Thread [ %d ] is finish\n",(int)tid[i]);
	}

	return ;
}

