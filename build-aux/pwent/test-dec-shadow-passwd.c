#include <prot.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/security.h>
#include <sys/types.h>
#include <unistd.h>

int
main () {
    struct pr_passwd *p;
    const char *pw;
    set_auth_parameters (0, 0);
    check_auth_parameters ();
    p = getprpwnam ("nobody");
    pw = p->ufld.fd_encrypt;
    return 0;
}
