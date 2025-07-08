#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>

int
main () {
    pam_strerror (PAM_SUCCESS);
    return 0;
}
