/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * subprocs.c --- choosing, spawning, and killing screenhacks.
 *
 * xscreensaver, Copyright (c) 1991-2003 Jamie Zawinski <jwz@jwz.org>
 * Modified:     Copyright (c) 2004 William Jon McCann <mccann@jhu.edu>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "config.h"

#include <ctype.h>
#include <signal.h> /* for the signal names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> /* sys/resource.h needs this for timeval */
#include <sys/wait.h> /* for waitpid() and associated macros */

#ifndef ESRCH
#include <errno.h>
#endif

#ifdef VMS
#include <processes.h>
#include <unixio.h> /* for close */
#include <unixlib.h> /* for getpid */
#define pid_t int
#define fork vfork
#endif /* VMS */

#include "subprocs.h"

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif

/* Semaphore to temporarily turn the SIGCHLD handler into a no-op.
   Don't alter this directly -- use block_sigchld() / unblock_sigchld().
*/
static int block_sigchld_handler = 0;


sigset_t
block_sigchld (void) {
    sigset_t child_set;
    sigemptyset (&child_set);
    sigaddset (&child_set, SIGCHLD);
    sigaddset (&child_set, SIGPIPE);
    sigprocmask (SIG_BLOCK, &child_set, 0);

    block_sigchld_handler++;

    return child_set;
}

void
unblock_sigchld (void) {
    sigset_t child_set;
    sigemptyset (&child_set);
    sigaddset (&child_set, SIGCHLD);
    sigaddset (&child_set, SIGPIPE);
    sigprocmask (SIG_UNBLOCK, &child_set, 0);

    block_sigchld_handler--;
}

int
signal_pid (int pid,
            int signal) {
    int status = -1;
    gboolean verbose = TRUE;

    if (block_sigchld_handler)
        /* This function should not be called from the signal handler. */
        abort ();

    block_sigchld (); /* we control the horizontal... */

    status = kill (pid, signal);

    if (verbose && status < 0) {
        if (errno == ESRCH) {
            g_message ("Child process %lu was already dead.",
                       (unsigned long) pid);
        } else {
            char buf[1024];
            snprintf (buf, sizeof (buf), "Couldn't kill child process %lu",
                      (unsigned long) pid);
            perror (buf);
        }
    }

    unblock_sigchld ();

    if (block_sigchld_handler < 0)
        abort ();

    return status;
}
