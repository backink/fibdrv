#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "bignum.h"

#define FIB_DEV "/dev/fibonacci"
#define THREAD_NUM 100

typedef struct {
    int func;
    int offset;
} thread_arg;

void *thread_func(void *arg)
{
    char buf[100000];
    thread_arg *args = (thread_arg *) arg;
    int func = args->func;
    int offset = args->offset;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        return NULL;
    }

    lseek(fd, func, SEEK_SET);
    if (!write(fd, NULL, 0)) {
        printf("not accurate function setting\n");
        return NULL;
    }

    lseek(fd, offset, SEEK_SET);
    read(fd, buf, sizeof(buf));
    printf("offset : %s\n", buf);

    close(fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t threads[THREAD_NUM];
    thread_arg args[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++) {
        args[i].func = atoi(argv[1]);
        args[i].offset = atoi(argv[0]) - i;
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, thread_func, &args[i]);
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
