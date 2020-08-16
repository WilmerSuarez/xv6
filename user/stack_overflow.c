#include "kernel/types.h"
#include "user.h"

void rec(int i) {
    printf(1, "%d(0x%x) \n", i, &i);
    rec(i+1);
}

int main(int argc, char *argv[]) {
    rec(0);
}