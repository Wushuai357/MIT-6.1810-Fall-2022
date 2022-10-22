#include "../kernel/types.h"
#include "./user.h"

int
main (void) {
    printf("%d\n", uptime());
    exit(0);
}