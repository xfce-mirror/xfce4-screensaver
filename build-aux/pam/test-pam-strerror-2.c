#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>

int
main () {
    pam_handle_t *pamh = 0;
    const char *s = pam_strerror (pamh, PAM_SUCCESS);
    return 0;
}
