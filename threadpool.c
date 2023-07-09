#include "threadpool.h"
#include<pthread.h>

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
    Task* taskQ;
    int queueCapacity;//容量
    int queueSize;//当前任务个数
    int queueFront;//队头->取数据
    int queueRear; //队尾->放数据

    pthread_t managerID;//管理者ID
    pthread_t* threadIDs;//工作的线程ID

    int minNum;//最小工作的线程数量
    int maxNum;//最大工作的线程数量
    int busyNum;//忙的线程个数
    int liveNum;//当前存活的线程数量
    int exitNum;//要销毁的线程个数

    pthread_mutex_t mutexpool;//锁整个线程池
    pthread_mutex_t mutexBusy;//锁busyNum变量
    pthread_cond_t notFull;//条件变量 任务队列是否满
    pthread_cond_t notEmpty;//条件变量 任务队列是否空了

    int shutdown;//是否要销毁线程池 1销毁 0 不销毁

};
