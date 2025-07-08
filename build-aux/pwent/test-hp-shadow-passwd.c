#include <hpsecurity.h>
#include <prot.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int
main () {
    struct s_passwd *p = getspwnam ("nobody");
    const char *pw = p->pw_passwd;
    return 0;
}
