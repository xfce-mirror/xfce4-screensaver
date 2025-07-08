#include <pwdadj.h>
#include <stdlib.h>
#include <sys/audit.h>
#include <sys/label.h>
#include <sys/types.h>
#include <unistd.h>

int
main () {
    struct passwd_adjunct *p = getpwanam ("nobody");
    const char *pw = p->pwa_passwd;
    return 0;
}
