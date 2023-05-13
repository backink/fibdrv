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
    long long sz;
    int offset = 1000; /* TODO: try test something bigger than the limit */
    // bn ret = BN_INIT;
    // ret.number = malloc(15 * sizeof(unsigned long long));

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    lseek(fd, offset, SEEK_SET);
    sz = read(fd, buf, sizeof(buf));
    if (sz == 0) {
        printf("read from module error\n");
        return 1;
    }
    printf("%s\n", buf);


    close(fd);
    return 0;
}
