#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadPool.h"

#define SYSERR "Error in system call"



/* each created thread, will run this function.
 * the function will loop until isStop updated, every iteration,
 * locks the mutex, dequeue a task and executes it. if not tasks
 * available, WAIT on the queue */
void *consumer(void *arg) {
    ThreadPool *tp = (ThreadPool *) arg;
    while (tp->isStop == 0) {
        pthread_mutex_lock(&tp->m);
        while ((tp->queue != NULL) && (osIsQueueEmpty(tp->queue))) {
            pthread_cond_wait(&tp->fill, &tp->m); //wait for signal that task is filled
        }
        if (tp->queue == NULL) {
            pthread_mutex_unlock(&tp->m);
            break;
        }
        Task *task = (Task *) osDequeue(tp->queue);
        if (osIsQueueEmpty(tp->queue)) {
            pthread_cond_signal(&tp->done); //if this task was the last, signal it
        }
        pthread_mutex_unlock(&tp->m);
        task->func(task->param);    //run the task
        free(task);

    }
    return 0;
}

/* Creates a new ThreadPool which runs  "numOfThreads" threads.
 * each thread runs the "consumer" funcion.
 * the ThreadPool also holds tasks queue, queue can be filled by using
 * the tpInsertTask function */
ThreadPool *tpCreate(int numOfThreads) {
    ThreadPool *tp = NULL;
    int err;
    tp = (ThreadPool *) malloc(sizeof(ThreadPool));
    if (tp == NULL) {  //if allocation failed
        write(2, SYSERR, sizeof(SYSERR) - 1);
        exit(-1);
    }

    OSQueue *q = osCreateQueue();
    if (q == NULL) { //if allocation failed
        free(tp);
        write(2, SYSERR, sizeof(SYSERR) - 1);
        exit(-1);
    }
    tp->queue = q;  //initialize queue

    pthread_t *tArr= (pthread_t *) malloc(numOfThreads * sizeof(pthread_t));
    if (tArr == NULL) { //if allocation failed
        osDestroyQueue(q);
        free(tp);
        write(2, SYSERR, sizeof(SYSERR) - 1);
        exit(-1);
    }
    tp->ths = tArr; //initialize pthreads array

    if (((err = pthread_cond_init(&tp->fill,NULL)) != 0) ||
            ((err = pthread_cond_init(&tp->done,NULL)) != 0) ||
            ((err = pthread_mutex_init(&tp->m,NULL)) != 0)) {
        free(tp->ths);
        osDestroyQueue(q);
        free (tp);
        write(2, SYSERR, sizeof(SYSERR) - 1);
        exit(-1);
    }

    tp->isStop = 0;
    int i;
    for (i = 0; i < numOfThreads; i++) { //each created thread runs "cosumer" func
        pthread_t tid;
        pthread_create(&tid, NULL, consumer, tp);
        tp->ths[i] = tid;
    }
    tp->threadsN = numOfThreads;
    tp->isDestroyed = 0;

    return tp;
}

//function to free all allocated memory from thread pool
void freeThreadPool(ThreadPool * tp) {
    void *data;
    int i;
    pthread_mutex_lock(&tp->m);
    tp->isStop = 1;
    tp->isDestroyed = 1;
    while ((data = osDequeue(tp->queue)) != NULL) { //free tasks
        free(data);
    }
    osDestroyQueue(tp->queue);
    tp->queue = NULL;
    pthread_cond_broadcast(&tp->fill); //wake all threads that are blocked !
    pthread_mutex_unlock(&tp->m);
    for (i = 0; i < tp->threadsN; ++i) { //wait for all threads funcs to finish
        pthread_join(tp->ths[i], NULL);
    }
    free(tp->ths);
    pthread_mutex_destroy(&tp->m);
    pthread_cond_destroy(&tp->done);
    pthread_cond_destroy(&tp->fill);
    free(tp);
}

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    Task *task = (Task *) malloc(sizeof(Task));
    if (task == NULL) {
        freeThreadPool(threadPool);
        write(2, SYSERR, sizeof(SYSERR) - 1);
        exit(-1);
    }
    task->func = computeFunc;
    task->param = param;
    pthread_mutex_lock(&threadPool->m);
    if (threadPool->isDestroyed || threadPool->queue == NULL) { //if tpDestroy function was called
        free(task);
        pthread_mutex_unlock(&threadPool->m);
        return -1;
    }
    osEnqueue(threadPool->queue, task);
    pthread_cond_broadcast(&threadPool->fill);  //broadcast to all the threads that wait for fill
    pthread_mutex_unlock(&threadPool->m);
    return 0;
}

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    void *data;
    if (threadPool->isDestroyed == 1) {    //if tpDestroy func already called
        return;
    }
    pthread_mutex_lock(&threadPool->m);
    threadPool->isDestroyed = 1;    //tells to stop inserting tasks
    if (!osIsQueueEmpty(threadPool->queue)) { //if more tasks in queue
        if (shouldWaitForTasks) {   //if should wait for tasks to end
            pthread_cond_wait(&threadPool->done, &threadPool->m); //wait for signal that all tasks done
            threadPool->isStop = 1;
        } else {  //case that should not wait for tasks
            threadPool->isStop = 1;
            while ((data = osDequeue(threadPool->queue)) != NULL) { //free tasks
                free(data);
            }
        }
    }
    pthread_mutex_unlock(&threadPool->m);
    freeThreadPool(threadPool);
}