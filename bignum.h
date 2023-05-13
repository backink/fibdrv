typedef struct {
    unsigned long long *number;
    unsigned int size;
    unsigned int capacity;
} bn;

#define BN_INIT                                  \
    {                                            \
        .number = NULL, .size = 0, .capacity = 0 \
    }

#define BN_INIT_ZERO                                                   \
    {                                                                  \
        .number = kmalloc(2 * sizeof(unsigned long long), GFP_KERNEL), \
        .size = 1, .capacity = 2                                       \
    }


void bn_init(bn **num);

void bn_init_u64(bn **num, unsigned long long ui);

void bn_free(bn *num);

void bn_copy(bn *dst, bn *src);

void bn_lshift(bn *out, bn *num);

void bn_sub(bn *out, bn *a, bn *b);

void bn_add(bn *out, bn *a, bn *b);

void bn_mul(bn *out, bn *a, bn *b);

void bn_swap(bn *a, bn *b);

char *bn_to_string(bn *num);
