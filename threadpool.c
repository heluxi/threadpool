#include "threadpool.h"
#include<unistd.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

//定义任务结构体
typedef struct Task
{
    void(*function)(void* arg);//定义一个函数指针 void*可以接受各种类型
    void* arg;

}Task;

//线程池结构体
struct ThreadPool
{
    //任务队列
    Task* taskQ;//使用指针来维护任务队列数组（动态分配内存）
    int queueCapacity;//容量
    int queueSize;//当前任务个数
    int queueFront;//队头->取数据
    int queueRear; //队尾->放数据

    pthread_t managerID;//管理者ID
    pthread_t* threadIDs;//工作的线程ID 使用指针来维护工作线程数组

    int minNum;//最小工作的线程数量
    int maxNum;//最大工作的线程数量
    int busyNum;//忙的线程个数
    int liveNum;//当前存活的线程数量
    int exitNum;//要销毁的线程个数

    pthread_mutex_t mutexpool;//锁整个线程池
    pthread_mutex_t mutexBusy;//锁busyNum变量
    pthread_cond_t notFull;//条件变量 用来判断任务队列是否满
    pthread_cond_t notEmpty;//条件变量 用来判断任务队列是否空了

    int shutdown;//是否要销毁线程池 1销毁 0 不销毁

};
//管理者线程的函数
void* manager(void* arg)
{

    ThreadPool* pool=(ThreadPool*)(arg);
    while(!pool->shutdown)
    {
        //每隔3s检测一次
        sleep(3);

        //取出线程池中任务的数量和当前线程的数量


        //取出忙的线程的数量


    }
    return NULL;
}

//工作线程的函数
void* worker(void* arg)
{
    ThreadPool* pool=(ThreadPool*)arg;
    while(1)//每个线程循环的读取任务队列里的任务 ，相当于消费者
    {
        pthread_mutex_lock(&pool->mutexpool);//访问线程池资源 要对线程池上锁

        //判断当前任务队列是否为空
        while(pool->queueSize==0&&!pool->shutdown)//为空就使用条件变量阻塞线程
        {
            //阻塞工作线程 等待被唤醒
            pthread_cond_wait(&pool->notEmpty,&pool->mutexpool);
        }
        //判断线程池是否被关闭
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexpool);//关闭线程池就解锁
            return NULL;
        }

        //读取任务队列里的函数
        Task task;
        task.function=pool->taskQ[pool->queueFront].function;//从头部取
        task.arg=pool->taskQ[pool->queueFront].arg;
        //移动头指针 将该数组维护为环型队列
        pool->queueFront=(pool->queueFront+1)% pool->queueCapacity;//头指针在队列中循环移动 当移动到最后一个再移动就重新遍历到开始
        pool->queueSize--;//当前任务个数-1

        pthread_mutex_unlock(&pool->mutexpool);

        //加锁 访问busyNum
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;//忙线程+1
        pthread_mutex_unlock(&pool->mutexBusy);

        //执行任务
        task.function(task.arg);//执行任务 通过函数指针调用函数
        free(task.arg);
        task.arg=NULL;
        printf("thread %ld end working.....\n",pthread_self());

        //给busynum加锁
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;//忙线程-1
        pthread_mutex_unlock(&pool->mutexBusy);

    }
    return NULL;
}

ThreadPool* threadPoolCreate(int max,int min,int Capacity)
{
    ThreadPool* Pool= (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
        //判断是否创建成功
        if(Pool==NULL)
        {
            printf("malloc threadPool fail....\n");
            break;
        }
        //动态创建一个数组用来管理工作线程
        Pool->threadIDs=(pthread_t *)malloc(sizeof(pthread_t)*max);
        if(Pool->threadIDs==NULL)
        {
            printf("malloc threadIDS fail....\n");
            break;
        }
        memset(Pool->threadIDs,0,sizeof(pthread_t)*max);//初始化为0

        Pool->minNum=min;
        Pool->maxNum=max;
        Pool->busyNum=0;
        Pool->liveNum=min;//默认先创建最小数量的线程
        Pool->exitNum=0;

        if(pthread_mutex_init(&Pool->mutexpool,NULL)!=0||
            pthread_mutex_init(&Pool->mutexBusy,NULL)!=0||
            pthread_cond_init(&Pool->notEmpty,NULL)!=0||
            pthread_cond_init(&Pool->notEmpty,NULL)!=0)
        {
            printf("mutex or cond fail\n");
            break;
        }
        //初始化任务队列
        Pool->taskQ=(Task*)malloc(sizeof(Task)*Capacity);//动态创建一个任务队列数组
        if(Pool->taskQ==NULL)
        {
            printf("malloc taskQ fail...\n");
            break;
        }
        Pool->queueCapacity=Capacity;
        Pool->queueSize=0;
        Pool->queueFront=0;
        Pool->queueRear=0;

        Pool->shutdown=0;

        //创建线程池中的线程
        //管理者线程
        pthread_create(&Pool->managerID,NULL,manager,Pool);

        //创建工作的线程

        for(int i=0;i<min;i++)
        {
            pthread_create(&Pool->threadIDs[i],NULL,worker,Pool);
        }

        return Pool;
    } while (0) ;

     //如果前面创建有问题就会break出来，这样我们就需要释放malloc出来的内存
    //释放malloc出来的内存
    if(Pool&&Pool->threadIDs)
    {
        free(Pool->threadIDs);
    }
    if(Pool&&Pool->taskQ)
    {
        free(Pool->taskQ);
    }
    if(Pool)
    {
        free(Pool);
    }



    return NULL;



}
