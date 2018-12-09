#include <stdio.h>
#include <unistd.h>
#include "sched.h"

static void testFunc(void)
{
	mthread_t self = mythread_self();
    //printf("###testFunc:%d\n", self);
    mythread_sleep(2000);
    auto f = []()
    {
    	mthread_t self = mythread_self();
        //printf("###subrun:%d\n", self);
        mythread_sleep(2000);
        //printf("###subTestFunc:%d\n", self);
    };
    auto cid = mythread::start_task(f, true);
    //printf("###create %d\n", cid);
    //printf("###Join coro:%d\n", cid);
    mythread_join(cid);
    //printf("###Join coro done:%d\n", cid);
}

int main()
{
	init_mythread(4);
    for (int i = 0; i < 100 ; i++)
    {
    	mthread_t id = mythread::start_task(testFunc, false);
    	//printf("###create %d\n", id);
    }
    sleep(1000);
    return 0;
}
