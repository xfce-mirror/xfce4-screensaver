[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/xfce4-screensaver/-/blob/master/COPYING)

# xfce4-screensaver

Xfce4-screensaver is a screen saver and locker that aims to have
simple, sane, secure defaults and be well integrated with the desktop.

This project is a port of [MATE Screensaver](https://github.com/mate-desktop/mate-screensaver),
itself a port of [GNOME Screensaver](https://gitlab.gnome.org/Archive/gnome-screensaver).
It has been tightly integrated with the Xfce desktop, utilizing Xfce
libraries and the Xfconf configuration backend.

----

### Features

 - Integration with the Xfce desktop per-monitor wallpaper
 - Locking down of configuration settings via Xfconf
 - Support for XScreensaver screensavers
 - (optional) Integration with ConsoleKit and Systemd
 - DBUS interface for limited control and querying screensaver status
 - Idle time and inhibition state are based on the X11 Screensaver extension
 - Shared styles with LightDM GTK+ Greeter
 - No GNOME or MATE dependencies. Requirements are lightweight and shared with Xfce.
 - Full translation support into many languages
 - User switching

 - Integration with the Xfce Desktop per-monitor wallpaper
 - Locking down of configuration settings via Xfconf
 - DBUS interface to limited screensaver interaction
 - Full translation support into many languages
 - Shared styles with LightDM GTK+ Greeter
 - Support for XScreensaver screensavers
 - User switching

### Known Issues

 - Allow embedding a keyboard into the window, /embedded-keyboard-enabled, may be non-functional. Onboard crashes when embedded.

### Homepage

[xfce4-screensaver documentation](https://docs.xfce.org/apps/xfce4-screensaver/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/xfce4-screensaver/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-screensaver source code](https://gitlab.xfce.org/apps/xfce4-screensaver)

### Download a Release Tarball

[Xfce4-screensaver archive](https://archive.xfce.org/src/apps/xfce4-screensaver)
    or
[Xfce4-screensaver tags](https://gitlab.xfce.org/apps/xfce4-screensaver/-/tags)

### Installation

See the file 'INSTALL'

From source code repository: 

    % cd xfce4-screensaver
    % ./autogen.sh --prefix=/usr --sysconfdir=/etc
    % make
    % make install

From release tarball:

    % tar xf xfce4-screensaver-<version>.tar.bz2
    % cd xfce4-screensaver-<version>
    % ./configure
    % make
    % make install

You may need to set your PAM auth type if it is not correctly detected.

`--with-pam-auth-type=<auth-type>   specify pam auth type (common or system)`

If you are using bsdauth or shadow auth, then you will need to make sure the
following is done after installation:

    chown root:root $libexecdir/xfce4-screensaver-dialog
    chmod +s $libexecdir/xfce4-screensaver-dialog

Example testing configuration:

    ./autogen.sh --disable-static --with-mit-ext --with-console-kit --enable-locking --enable-debug --sysconfdir=/etc

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/xfce4-screensaver/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

In the bug report please include information about your system, if possible:

 - What operating system and version
 - What version of xfce4-screensaver, i.e. the output of the `xfce4-screensaver-command --version` command

If you want to debug your installation you may also be able to get meaningful debug output when starting xfce4-screensaver from the debug script:
`./src/debug-screensaver.sh`
