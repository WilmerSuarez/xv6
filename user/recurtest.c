// Big recursion test
#include "kernel/types.h"
#include "user.h"

// 128KB stack array block
#define BLOCK 131072

int fun(int n, int max)
{
    char v[BLOCK];
    int i;
    for (i = 0; i < BLOCK / 3; i++) {
        v[i*3] = 0xe5;
        v[i*3+1] = 0x96;
        v[i*3+2] = 0xb5;
    }
    v[32] = 0;
    printf(1, "This is %d\n", n);
    printf(1, "%s\n", v);
    if (n >= max) {
        return n;
    }
    return fun(n + 1, max);
}

int main(int argc, char const *argv[])
{
    int maxdepth = 30;
    int customdepth = 0;
    if (argc > 1) {
        customdepth = atoi(argv[1]);
        if (customdepth > 0) {
            maxdepth = customdepth;
        }
    }
    fun(1, maxdepth);
    exit(0);
    return 0;
}
