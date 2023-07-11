#include "threadpool.h"
#include<unistd.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

const int  Number=2;//一次性添加2个线程

//定义任务结构体
typedef struct Task
{
    void(*function)(void* arg);//定义一个函数指针 void*可以接受各种类型
    void* arg;

}Task;

//线程池结构体
//线程池里要有一个管理者线程，用来管理线程池里的线程，当线程池里的线程太少而任务队列中的任务太多会增加线程，反之会去减少线程
//使用条件变量和互斥锁来控制线程同步

struct ThreadPool
{
    //任务队列
    Task* taskQ;//使用指针来维护任务队列数组（动态分配内存）
    int queueCapacity;//任务队列容量
    int queueSize;//当前队伍里任务个数
    int queueFront;//队头->取数据
    int queueRear; //队尾->放数据

    pthread_t managerID;//管理者线程ID
    pthread_t* threadIDs;//工作的线程ID 使用指针来维护工作线程数组

    int minNum;//最小工作的线程数量
    int maxNum;//最大工作的线程数量
    int busyNum;//忙的线程个数
    int liveNum;//当前存活的线程数量
    int exitNum;//要销毁的线程个数

    pthread_mutex_t mutexpool;//锁整个线程池
    pthread_mutex_t mutexBusy;//锁busyNum变量 因为busyNum经常被线程访问 所有单独为其加锁 提供操作效率
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
        pthread_mutex_lock(&pool->mutexpool);

        int queueSize=pool->queueSize;
        int liveNum=pool->liveNum;

        pthread_mutex_unlock(&pool->mutexpool);

        //取出忙的线程的数量
        pthread_mutex_lock(&pool->mutexBusy);

        int busyNum=pool->busyNum;

        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程 当任务的个数>存活的线程个数 && 存活的线程数<最大线程数


        if(queueSize>liveNum && liveNum<pool->maxNum)//每次添加2个
        {
            int counter=0;
            pthread_mutex_lock(&pool->mutexpool);
            for(int i=0;(i<pool->maxNum) && (counter<Number) && (pool->liveNum<pool->maxNum); i++)
            {
                if(pool->threadIDs[i]==0)
                {
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    counter++;
                    pool->liveNum++;
                }

            }

            pthread_mutex_unlock(&pool->mutexpool);
        }

        //销毁线程 忙的线程*2 <存活的线程 && 存活的线程>最小线程数
        //一次销毁两个
        if(busyNum*2<liveNum&&liveNum>pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexpool);
            pool->exitNum=Number;
            pthread_mutex_unlock(&pool->mutexpool);
            //让工作的线程自杀
            //没有工作的线程阻塞在条件变量上 将其唤醒 让其自杀
            for(int i=0;i<Number;i++)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }


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

            //判断是不是要销毁
            if(pool->exitNum>0)
            {
                pool->exitNum--;
                if(pool->liveNum>pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexpool);
                    threadExit(pool);
                }

            }
        }
        //判断线程池是否被关闭
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexpool);//关闭线程池就解锁
            threadExit(pool);
        }

        //读取任务队列里的函数
        Task task;
        task.function=pool->taskQ[pool->queueFront].function;//从头部取
        task.arg=pool->taskQ[pool->queueFront].arg;
        //移动头指针 将该数组维护为环型队列
        pool->queueFront=(pool->queueFront+1)% pool->queueCapacity;//头指针在队列中循环移动 当移动到最后一个再移动就重新遍历到开始
        pool->queueSize--;//当前任务个数-1
        //唤醒生产者
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexpool);


        //加锁 访问busyNum
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;//忙线程+1
        pthread_mutex_unlock(&pool->mutexBusy);

        printf("thread %ld start working.....\n",pthread_self());
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

//创建线程池
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
//把要销毁的线程的数组元素恢复为0
void threadExit(ThreadPool *pool)
{
    pthread_t tid=pthread_self();
    for (int i = 0; i < pool->maxNum; ++i) {
        if(pool->threadIDs[i]==tid)
        {
            pool->threadIDs[i]=0;
            printf("threadExit() called,%ld exiting....\n",tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mutexpool);
    //判断任务队列已经满了
    while(pool->queueSize==pool->queueCapacity&&!pool->shutdown)
    {
        //阻塞生产者线程
        pthread_cond_wait(&pool->notFull,&pool->mutexpool);

    }
    if(pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexpool);
        return;
    }
    //添加任务
    pool->taskQ[pool->queueRear].function=func;
    pool->taskQ[pool->queueRear].arg=arg;
    pool->queueRear=(pool->queueRear+1)%pool->queueCapacity;
    pool->queueSize++;
    //唤醒阻塞在条件变量的消费者
    pthread_cond_signal(&pool->notEmpty);

    pthread_mutex_unlock(&pool->mutexpool);
}

int threadPoolBusyNum(ThreadPool *pool)
{

    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum=pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;

}

int threadPoolAliveNum(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->mutexpool);
    int aliveNum=pool->liveNum;
    pthread_mutex_unlock(&pool->mutexpool);
    return aliveNum;
}

int ThreadPoolDestroy(ThreadPool *pool)
{
    if(pool==NULL)
    {
        return -1;
    }
    //关闭线程
    pool->shutdown=1;
    //阻塞回收管理者线程
    pthread_join(pool->managerID,NULL);

    //唤醒阻塞的消费者线程
    for(int i=0;i<pool->liveNum;++i)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    //释放堆内存
    if(pool->taskQ)
    {
        free(pool->taskQ);
    }
    if(pool->threadIDs)
    {
        free(pool->threadIDs);
    }
    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool= NULL;

    return 0;
}
