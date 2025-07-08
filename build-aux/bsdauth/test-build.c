#include <bsd_auth.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int
main () {
    int ok = auth_userokay ("x", 0, "x", "x");
    return 0;
}
