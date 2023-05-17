#include "bignum.h"
#include <linux/slab.h>
#include <linux/string.h>

void bn_init(bn **num)
{
    *num = kmalloc(sizeof(bn), GFP_KERNEL);
    (*num)->number = kmalloc(2 * sizeof(unsigned long long), GFP_KERNEL);
    memset((*num)->number, 0, 2 * sizeof(unsigned long long));
    (*num)->size = 0;
    (*num)->capacity = 2;
}

void bn_set_bn(bn *dst, bn *src)
{
    kfree(dst->number);
    dst->number = src->number;
    dst->size = src->size;
    dst->capacity = src->capacity;
}

void bn_copy(bn *dst, bn *src)
{
    kfree(dst->number);

    dst->number =
        kmalloc(src->capacity * sizeof(unsigned long long), GFP_KERNEL);
    memset(dst->number, 0, src->capacity * sizeof(unsigned long long));
    for (int i = 0; i < src->size; i++)
        dst->number[i] = src->number[i];
    dst->size = src->size;
    dst->capacity = src->capacity;
}

void bn_init_u64(bn **num, unsigned long long ui)
{
    bn_init(num);
    (*num)->number[0] = ui;
    (*num)->size = 1;
}

void bn_free(bn *num)
{
    kfree(num->number);
    kfree(num);
}

void bn_swap(bn *a, bn *b)
{
    bn tmp = *a;
    *a = *b;
    *b = tmp;
}


static void bn_extend(bn *target)
{
    unsigned long long *new_num =
        kmalloc(target->capacity * 2 * sizeof(unsigned long long), GFP_KERNEL);
    memset(new_num, 0, target->capacity * sizeof(unsigned long long));
    for (int i = 0; i < target->size; i++) {
        new_num[i] = target->number[i];
    }
    kfree(target->number);
    target->number = new_num;
    target->capacity *= 2;
}

void bn_add(bn *out, bn *a, bn *b)
{
    int short_size, long_size;
    bn *larger;
    if (b->size > a->size) {
        short_size = a->size;
        long_size = b->size;
        larger = b;
    } else {
        short_size = b->size;
        long_size = a->size;
        larger = a;
    }

    bn tmp = BN_INIT;
    tmp.number =
        kmalloc(larger->capacity * sizeof(unsigned long long), GFP_KERNEL);
    memset(tmp.number, 0, larger->capacity * sizeof(unsigned long long));
    tmp.capacity = larger->capacity;

    int carry = 0;
    for (int i = 0; i < short_size; i++) {
        unsigned long long x = a->number[i];
        unsigned long long y = b->number[i];
        tmp.number[i] = x + y + carry;
        if (y > (~0UL - x - carry)) {
            carry = 1;
        } else {
            carry = 0;
        }
    }
    for (int i = short_size; i < long_size; i++) {
        unsigned long long x = larger->number[i];
        if (x == (~0UL)) {
            tmp.number[i] = 0;
            carry = 1;
        } else {
            tmp.number[i] = x + carry;
            carry = 0;
        }
    }
    tmp.size = larger->size;
    if (carry) {
        if (tmp.size == tmp.capacity)
            bn_extend(&tmp);
        tmp.number[larger->size] = 1;
        tmp.size++;
    }
    bn_set_bn(out, &tmp);
}

void bn_sub(bn *out, bn *a, bn *b)
{
    int short_len = b->size;
    bn *larger = a, *smaller = b;

    bn tmp = BN_INIT;
    bn_copy(&tmp, larger);

    for (int i = 0; i < short_len; i++) {
        if (tmp.number[i] < smaller->number[i] && i + 1 < tmp.capacity) {
            int j = i + 1;
            while (j < tmp.capacity && tmp.number[j] == 0) {
                tmp.number[j++]--;
            }
            if (j < tmp.capacity)
                tmp.number[j]--;
        }
        tmp.number[i] -= smaller->number[i];
    }
    bn_set_bn(out, &tmp);
}

void bn_lshift(bn *out, bn *num)
{
    if (!num)
        return;

    bn tmp = BN_INIT;
    bn_copy(&tmp, num);

    if (tmp.number[tmp.capacity - 1] & (1UL << 63)) {
        bn_extend(&tmp);
    }

    int tail = 0, head;
    for (int i = 0; i < tmp.size; i++) {
        if (tmp.number[i] & (1UL << 63))
            head = 1;
        else
            head = 0;
        tmp.number[i] <<= 1;
        tmp.number[i] += tail;
        tail = head;
    }
    if (tail) {
        tmp.number[num->size] = 1;
        tmp.size++;
    }
    bn_set_bn(out, &tmp);
}

void bn_mul(bn *out, bn *a, bn *b)
{
    bn tmp = BN_INIT;
    bn tmp2 = BN_INIT;
    bn ret = BN_INIT;
    ret.number = kmalloc(2 * sizeof(unsigned long long), GFP_KERNEL);
    memset(ret.number, 0, 2 * sizeof(unsigned long long));
    ret.size = 1;
    ret.capacity = 2;

    bn_copy(&tmp, a);
    bn_copy(&tmp2, b);

    for (int i = 0; i < tmp.size; i++) {
        for (int j = 0; j < 64; j++) {
            if (tmp.number[i] & (1UL << j)) {
                bn_add(&ret, &ret, &tmp2);
            }
            bn_lshift(&tmp2, &tmp2);
        }
    }
    kfree(tmp.number);
    kfree(tmp2.number);
    bn_set_bn(out, &ret);
}

char *bn_to_string(bn *num)
{
    int len = (8 * sizeof(unsigned long long) * num->size) / 3 + 2;

    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;
    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = num->size - 1; i >= 0; i--) {
        for (unsigned long long j = 1UL << 63; j; j >>= 1) {
            int carry = !!(j & num->number[i]);
            for (int k = len - 2; k >= 0; k--) {
                s[k] += s[k] - '0' + carry;
                carry = s[k] > '9';
                if (carry)
                    s[k] -= 10;
            }
        }
    }

    while (*p == '0' && *(p + 1) != '\0')
        p++;
    memmove(s, p, strlen(p) + 1);

    return s;
}
