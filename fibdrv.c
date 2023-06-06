#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include "bignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 10000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t start_time, end_time;
static s64 elapsed_ns;
static void (*fib_func)(unsigned long long, char *);


static long long fib_sequence(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static long long fib_dp(long long k)
{
    if (k < 2)
        return k;

    long long a = 0;
    long long b = 1;

    for (int i = 2; i <= k; i++) {
        long long tmp = a + b;
        a = b;
        b = tmp;
    }

    return b;
}

static long long fast_doubling_rec(long long k)
{
    if (k < 2)
        return k;
    long long fk = fast_doubling_rec(k >> 1);
    long long fk_1 = fast_doubling_rec((k >> 1) + 1);

    if (k & 1)
        return fk * fk + fk_1 * fk_1;
    else
        return fk * ((fk_1 << 1) - fk);
}

static long long fast_doubling_iter(long long k)
{
    if (k <= 2)
        return !!k;
    uint8_t count = 63 - __builtin_clzll(k);
    uint64_t fib_k = 1, fib_k1 = 1;

    for (uint64_t i = count, fib_2k, fib_2k1; i-- > 0;) {
        fib_2k = fib_k * ((fib_k1 << 1) - fib_k);
        fib_2k1 = fib_k * fib_k + fib_k1 * fib_k1;

        if (k & (1 << (i - 1))) {
            fib_k = fib_2k1;
            fib_k1 = fib_2k + fib_2k1;
        } else {
            fib_k = fib_2k;
            fib_k1 = fib_2k1;
        }
    }
    return fib_k;
}

void fib_bn_dp(unsigned long long target, char *buf)
{
    char *ret;
    bn *a;
    bn_init_u64(&a, 1);
    bn *b;
    bn_init_u64(&b, 1);
    bn *c;
    bn_init_u64(&c, 0);
    if (target <= 2)
        goto end;

    for (int i = 3; i <= target; i++) {
        bn_add(c, a, b);
        bn_swap(a, c);
        bn_swap(b, c);
    }

end:
    ret = bn_to_string(c);
    copy_to_user(buf, ret, strlen(ret) + 1);
    kfree(ret);
    bn_free(a);
    bn_free(b);
    bn_free(c);
}


void bn_fast_doubling(unsigned long long target, char *buf)
{
    char *ret;
    bn *fib_n;
    bn_init_u64(&fib_n, 1);
    bn *fib_n1;
    bn_init_u64(&fib_n1, 1);
    bn *fib_2n;
    bn_init_u64(&fib_2n, 0);
    bn *fib_2n1;
    bn_init_u64(&fib_2n1, 0);

    if (target <= 2)
        goto end;

    int count = 63 - __builtin_clzll(target);

    for (unsigned long long i = count; i-- > 0;) {
        bn_lshift(fib_2n, fib_n1);
        bn_sub(fib_2n, fib_2n, fib_n);
        bn_mul(fib_2n, fib_2n, fib_n);

        bn_mul(fib_2n1, fib_n, fib_n);
        bn_mul(fib_n1, fib_n1, fib_n1);
        bn_add(fib_2n1, fib_2n1, fib_n1);
        if (target & (1UL << i)) {
            bn_copy(fib_n, fib_2n1);
            bn_add(fib_n1, fib_2n, fib_2n1);
        } else {
            bn_copy(fib_n, fib_2n);
            bn_copy(fib_n1, fib_2n1);
        }
    }
end:
    ret = bn_to_string(fib_n);
    copy_to_user(buf, ret, strlen(ret) + 1);
    kfree(ret);
    bn_free(fib_n);
    bn_free(fib_n1);
    bn_free(fib_2n);
    bn_free(fib_2n1);
}

static long long fib_time_proxy(unsigned long long k, char *buf)
{
    start_time = ktime_get();
    fib_func(k, buf);
    end_time = ktime_get();
    elapsed_ns = ktime_to_ns(ktime_sub(end_time, start_time));

    return (long long) elapsed_ns;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return fib_time_proxy(*offset, buf);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (*offset) {
    case 0:
        fib_func = fib_bn_dp;
        break;
    case 1:
        fib_func = bn_fast_doubling;
        break;
    default:
        return 0;
    }
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
