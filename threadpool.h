#ifndef THREADPOOL_H
#define THREADPOOL_H

typedef struct ThreadPool ThreadPool;
//创建线程池并初始化
ThreadPool* threadPoolCreate(int max,int min,int Capacity);//初始化最大线程个数 最小线程个数，任务队列的容量


//销毁线程池



//给线程池添加任务

//获取线程池中工作的线程个数

//获取线程池中活着的线程个数


//工作线程的函数
void* worker(void* arg);

//管理者线程的函数
void* manager(void* arg);
#endif // THREADPOOL_H
