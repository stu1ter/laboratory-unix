#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    int             ready;
    int             stopping;
    void           *data;
} monitor_t;

static monitor_t mon = {
    .mutex     = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER,
    .ready     = 0,
    .stopping  = 0,
    .data      = NULL
};

static void *producer(void *arg)
{
    (void)arg;
    for (int i = 0; i < 5; ++i) {
        sleep(1);

        int *payload = malloc(sizeof(int));
        *payload = i;

        pthread_mutex_lock(&mon.mutex);

        while (mon.ready && !mon.stopping) {
            pthread_mutex_unlock(&mon.mutex);
            usleep(1000);
            pthread_mutex_lock(&mon.mutex);
        }
        if (mon.stopping) {
            free(payload);
            pthread_mutex_unlock(&mon.mutex);
            break;
        }

        mon.data  = payload;
        mon.ready = 1;

        printf("[Поставщик] Событие %d отправлено (data=%p)\n", i, (void*)payload);

        pthread_cond_signal(&mon.condition);
        pthread_mutex_unlock(&mon.mutex);
    }

    pthread_mutex_lock(&mon.mutex);
    mon.stopping = 1;
    pthread_cond_broadcast(&mon.condition);
    pthread_mutex_unlock(&mon.mutex);

    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    while (true) {
        pthread_mutex_lock(&mon.mutex);

        while (!mon.ready && !mon.stopping)
            pthread_cond_wait(&mon.condition, &mon.mutex);

        if (mon.stopping && !mon.ready) {
            pthread_mutex_unlock(&mon.mutex);
            break;
        }

        int *payload = (int *)mon.data;
        mon.data  = NULL;
        mon.ready = 0;

        pthread_mutex_unlock(&mon.mutex);
        printf("[Потребитель] Событие %d обработано (data=%p)\n", *payload, (void*)payload);
        free(payload);
    }

    return NULL;
}

int main(void)
{
    pthread_t prod, cons;

    pthread_create(&cons, NULL, consumer, NULL);
    pthread_create(&prod, NULL, producer, NULL);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    pthread_mutex_destroy(&mon.mutex);
    pthread_cond_destroy(&mon.condition);
    return 0;
} 
