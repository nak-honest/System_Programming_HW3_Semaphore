#include "TestCase3.h"

thread_t tidArray[TC3_THREAD_NUM];

void* Tc3ThreadProc(void* param)
{
	thread_t tid = 0;

	int currentNum=(*((int*)param));
	tid = thread_self();

	int i;
	for(i=0;i<5;i++){
		printf("Tc3ThreadProc: my thread id (%d), arg is (%d), thread_cputime is (%d)\n", (int)tid, *((int*)param),thread_cputime());
		if(i==1&&currentNum-1>=0){
			thread_resume(tidArray[currentNum-1]);
		}
		sleep(2);
	}
	thread_exit();
	return NULL;
}


void TestCase3(void)
{

	int arr[TC3_THREAD_NUM]={0,1,2,3,4,5};

	for(int i=0;i<TC3_THREAD_NUM;i++){
		thread_create(&tidArray[i], NULL, (void*)Tc3ThreadProc,(void*) &(arr[i]));	
		thread_suspend(tidArray[i]);
	}

	thread_resume(tidArray[5]);

	thread_join(tidArray[0]);
	printf("Thread [ %d ] is finish\n",(int)tidArray[0]);

	assert(0);

	return ;
}
