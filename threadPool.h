#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <bits/types/sigevent_t.h>
#include <sys/types.h>
#include "osqueue.h"



typedef struct thread_pool
{
 volatile int isDestroyed;  //boolean indicating that pool is in destruction
 volatile int isStop;   //boolean tells the threads to stop running new tasks
 pthread_t *ths;    //an threads array
 int threadsN;  //number of threads the pool contains
 OSQueue *queue;    //queue of tasks, threads need to run
 pthread_mutex_t m; //mutex for synchronizing threads
 pthread_cond_t done, fill; //condition variables used to signal for events

}ThreadPool;

typedef struct Task {
    void (*func)(void*);    //function pointer needs to be executed
    void* param;    //parameters for the function
}Task;



ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
