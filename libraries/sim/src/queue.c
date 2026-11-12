#include <stddef.h>
#include <simulator/queue.h>
#ifdef SDK_PORT
#include <pthread.h>
static pthread_mutex_t s_queueMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void* SIM_queue_read(SIM_queue_t *queue) {
    void *handle = NULL;
#ifdef SDK_PORT
    pthread_mutex_lock(&s_queueMutex);
#endif
    if (queue->tail != queue->head) {
        handle = queue->data[queue->tail];
        queue->data[queue->tail] = NULL;
        queue->tail = (queue->tail + 1) % queue->size;
    }
#ifdef SDK_PORT
    pthread_mutex_unlock(&s_queueMutex);
#endif
    return handle;
}

int SIM_queue_write(SIM_queue_t *queue, void* handle) {
    int result = -1;
#ifdef SDK_PORT
    pthread_mutex_lock(&s_queueMutex);
#endif
    if (((queue->head + 1) % queue->size) != queue->tail) {
        queue->data[queue->head] = handle;
        queue->head = (queue->head + 1) % queue->size;
        result = 0;
    }
#ifdef SDK_PORT
    pthread_mutex_unlock(&s_queueMutex);
#endif
    return result;
}