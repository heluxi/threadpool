#include <stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include"threadpool.h"

void taskFunction(void* arg)
{
    int *num=(int*)arg;
    printf("thread %ld is working, number = %d,\n",pthread_self(),*num);
    sleep(1);
}
int main()
{

    //创建线程池
    ThreadPool *pool=threadPoolCreate(3,10,100);
    //添加任务函数
    for(int i=0;i<100;i++)
    {
        int* num=(int*)malloc(sizeof(int));
        *num=i+100;
        threadPoolAdd(pool,taskFunction,num);
    }

    sleep(30);

    //销毁线程
    ThreadPoolDestroy(pool);
    return 0;
}
