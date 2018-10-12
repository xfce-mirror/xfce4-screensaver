:exclamation: UNDER CONSTRUCTION :exclamation:

This project is a work-in-progress port of the Mate Screensaver.

xfce4-screensaver
=================

xfce4-screensaver is a screen saver and locker that aims to have
simple, sane, secure defaults and be well integrated with the desktop.
It is designed to support:

 - the ability to lock down configuration settings
 - translation into many languages
 - user switching


Known Issues
============

Currently none.

Installation
============

See the file 'INSTALL'

`./autogen.sh --prefix=/usr --sysconfdir=/etc`

(For testing, we are using:
 `./autogen.sh --disable-static --with-mit-ext --with-console-kit --enable-locking --enable-debug --sysconfdir=/etc`
)

`make`
`sudo make install`


How to report bugs
==================

For the time being, please report bugs here on GitHub:
    https://github.com/bluesabre/xfce4-screensaver/issues

You will need to create an account if you don't have one already.

In the bug report please include information about your system, if possible:

 - What operating system and version
 - What version of xfce4-screensaver, i.e. the output of the `xfce4-screensaver-command --version` command

If you want to debug your installation you may also be able to get meaningful debug output when starting xfce4-screensaver from the debug script:
`./src/debug-screensaver.sh`
