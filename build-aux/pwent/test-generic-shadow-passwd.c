#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int
main () {
    struct spwd *p = getspnam ("nobody");
    const char *pw = p->sp_pwdp;
    return 0;
}
