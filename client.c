#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "bignum.h"

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[100000];
    long long time_ns;
    int offset = 10000; /* TODO: try test something bigger than the limit */
    // bn ret = BN_INIT;
    // ret.number = malloc(15 * sizeof(unsigned long long));

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, offset, SEEK_SET);
    time_ns = read(fd, buf, sizeof(buf));
    printf("%.2f\n", time_ns * 1e-6);
    // printf("%s\n", buf);


    close(fd);
    return 0;
}
