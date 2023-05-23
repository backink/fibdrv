#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "bignum.h"

#define FIB_DEV "/dev/fibonacci"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("not enough argument\n");
        return -1;
    }
    char buf[100000];
    long long time_ns;
    int offset = atoi(argv[2]);
    int func = atoi(argv[1]);
    /* 0 -> dp, 1 -> fast_doubling */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, func, SEEK_SET);
    if (!write(fd, NULL, 0)) {
        printf("not accurate function setting\n");
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    time_ns = read(fd, buf, sizeof(buf));
    printf("%lld\n", time_ns);

    close(fd);
    return 0;
}
