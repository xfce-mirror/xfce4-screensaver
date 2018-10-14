Xfce Screensaver
================

xfce4-screensaver is a screen saver and locker that aims to have
simple, sane, secure defaults and be well integrated with the desktop.

This project is a port of [MATE Screensaver](https://github.com/mate-desktop/mate-screensaver),
itself a port of [GNOME Screensaver](https://gitlab.gnome.org/Archive/gnome-screensaver).
It has been tightly integrated with the Xfce desktop, utilizing Xfce
libraries and the Xfconf configuration backend.

Features
========

 - Integration with the Xfce Desktop per-monitor wallpaper
 - Locking down of configuration settings via Xfconf
 - DBUS interface to limited screensaver interaction
 - Full translation support into many languages
 - Shared styles with LightDM GTK+ Greeter
 - Support for XScreensaver screensavers
 - User switching

Known Issues
============

 - Theming issues with the Adwaita GTK theme

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
