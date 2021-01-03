/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

xfcebg.c: Object for the desktop background.

Copyright (C) 2000 Eazel, Inc.
Copyright (C) 2007-2008 Red Hat, Inc.
Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>
Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this program; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA 02110-1301, USA.

Derived from eel-background.c and eel-gdk-pixbuf-extensions.c by
Darin Adler <darin@eazel.com> and Ramiro Estrugo <ramiro@eazel.com>

Authors: Soren Sandmann <sandmann@redhat.com>
         Jasmine Hassan <jasmine.aura@gmail.com>

*/

#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gio/gio.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <cairo.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <xfce-bg.h>
#include <xfconf/xfconf.h>

#define XFCE_BG_CACHE_DIR "xfce/background"
#define XFCE_BG_FALLBACK_IMG "xfce-teal.jpg"

/* We keep the large pixbufs around if the next update
   in the slideshow is less than 60 seconds away */
#define KEEP_EXPENSIVE_CACHE_SECS 60

typedef struct _SlideShow SlideShow;
typedef struct _Slide Slide;

struct _Slide {
    double    duration; /* in seconds */
    gboolean  fixed;

    GSList   *file1;
    GSList   *file2; /* NULL if fixed is TRUE */
};

typedef struct _FileSize FileSize;
struct _FileSize {
    gint  width;
    gint  height;

    char *file;
};

/* This is the size of the GdkRGB dither matrix, in order to avoid
 * bad dithering when tiling the gradient
 */
#define GRADIENT_PIXMAP_TILE_SIZE 128

typedef struct FileCacheEntry FileCacheEntry;
#define CACHE_SIZE 4

/*
 *   Implementation of the XfceBG class
 */
struct _XfceBG {
    GObject          parent_instance;
    char            *filename;
    XfceBGPlacement  placement;
    XfceBGColorType  color_type;
    GdkRGBA          primary;
    GdkRGBA          secondary;
    gboolean         is_enabled;

    GFileMonitor    *file_monitor;

    guint            changed_id;
    guint            transitioned_id;
    guint            blow_caches_id;

    /* Cached information, only access through cache accessor functions */
    SlideShow       *slideshow;
    time_t           file_mtime;
    GdkPixbuf       *pixbuf_cache;
    int              timeout_id;

    GList           *file_cache;
};

struct _XfceBGClass {
    GObjectClass parent_class;
};

enum {
    CHANGED,
    TRANSITIONED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

G_DEFINE_TYPE(XfceBG, xfce_bg, G_TYPE_OBJECT)

/* Pixbuf utils */
static GdkPixbuf * pixbuf_scale_to_fit      (GdkPixbuf        *src,
                                             int               max_width,
                                             int               max_height);
static GdkPixbuf * pixbuf_scale_to_min      (GdkPixbuf        *src,
                                             int               min_width,
                                             int               min_height);

static void        pixbuf_draw_gradient     (GdkPixbuf        *pixbuf,
                                             gboolean          horizontal,
                                             GdkRGBA          *c1,
                                             GdkRGBA          *c2,
                                             GdkRectangle     *rect);

static void        pixbuf_tile              (GdkPixbuf        *src,
                                             GdkPixbuf        *dest);
static void        pixbuf_blend             (GdkPixbuf        *src,
                                             GdkPixbuf        *dest,
                                             int               src_x,
                                             int               src_y,
                                             int               src_width,
                                             int               src_height,
                                             int               dest_x,
                                             int               dest_y,
                                             double            alpha);

/* Cache */
static GdkPixbuf * get_pixbuf_for_size      (XfceBG           *bg,
                                             gint              monitor,
                                             int               best_width,
                                             int               best_height);
static void        clear_cache              (XfceBG           *bg);
static gboolean    is_different             (XfceBG           *bg,
                                             const char       *filename);
static time_t      get_mtime                (const char       *filename);
static SlideShow * get_as_slideshow         (XfceBG           *bg,
                                             const char       *filename);
static Slide *     get_current_slide        (SlideShow        *show,
                                             double           *alpha);

static SlideShow * read_slideshow_file      (const char       *filename,
                                             GError          **err);
static SlideShow * slideshow_ref            (SlideShow        *show);
static void        slideshow_unref          (SlideShow        *show);

static FileSize  * find_best_size           (GSList           *sizes,
                                             gint              width,
                                             gint              height);

static void        xfce_bg_set_color        (XfceBG           *bg,
                                             XfceBGColorType   type,
                                             GdkRGBA          *primary,
                                             GdkRGBA          *secondary);

static void        xfce_bg_load_from_xfconf (XfceBG           *bg,
                                             XfconfChannel    *channel,
                                             GdkMonitor       *monitor);

static void        xfce_bg_set_placement    (XfceBG           *bg,
                                             XfceBGPlacement   placement);

static void        xfce_bg_set_filename     (XfceBG           *bg,
                                             const char       *filename);

static void        color_from_rgba_array    (XfconfChannel    *channel,
                                             const gchar      *property,
                                             GdkRGBA          *colorp) {
    gdouble r, g, b, a;

    /* If all else fails use Xfdesktop's default */
    gdk_rgba_parse (colorp, "#152233");

    if (!xfconf_channel_has_property (channel, property))
        return;

    xfconf_channel_get_array(channel,
                             property,
                             G_TYPE_DOUBLE, &r,
                             G_TYPE_DOUBLE, &g,
                             G_TYPE_DOUBLE, &b,
                             G_TYPE_DOUBLE, &a,
                             G_TYPE_INVALID);

    colorp->red = r;
    colorp->green = g;
    colorp->blue = b;
    colorp->alpha = a;
}

static void        color_from_color_array   (XfconfChannel    *channel,
                                             const gchar      *property,
                                             GdkRGBA          *colorp) {
    guint   rc, gc, bc, ac;

    /* If all else fails use Xfdesktop's default */
    gdk_rgba_parse (colorp, "#152233");

    if (!xfconf_channel_has_property (channel, property))
        return;

    xfconf_channel_get_array(channel,
                             property,
                             G_TYPE_UINT, &rc,
                             G_TYPE_UINT, &gc,
                             G_TYPE_UINT, &bc,
                             G_TYPE_UINT, &ac,
                             G_TYPE_INVALID);

    colorp->red = (gdouble) rc / 65535;
    colorp->green = (gdouble) gc / 65535;
    colorp->blue = (gdouble) bc / 65535;
    colorp->alpha = (gdouble) ac / 65535;
}

static gboolean
do_changed (gpointer user_data) {
    XfceBG *bg = user_data;

    bg->changed_id = 0;

    g_signal_emit (G_OBJECT (bg), signals[CHANGED], 0);

    return FALSE;
}

static void
queue_changed (XfceBG *bg) {
    if (bg->changed_id > 0) {
        g_source_remove (bg->changed_id);
    }

    bg->changed_id = g_timeout_add_full (G_PRIORITY_LOW, 100, do_changed, bg, NULL);
}

static gboolean
do_transitioned (gpointer user_data) {
    XfceBG *bg = user_data;
    bg->transitioned_id = 0;

    if (bg->pixbuf_cache) {
        g_object_unref (bg->pixbuf_cache);
        bg->pixbuf_cache = NULL;
    }

    g_signal_emit (G_OBJECT (bg), signals[TRANSITIONED], 0);

    return FALSE;
}

static void
queue_transitioned (XfceBG *bg) {
    if (bg->transitioned_id > 0) {
        g_source_remove (bg->transitioned_id);
    }

    bg->transitioned_id = g_timeout_add_full (G_PRIORITY_LOW, 100, do_transitioned, bg, NULL);
}

static gchar *
find_system_backgrounds (void) {
    const gchar * const *dirs;
    gint                 i;

    dirs = g_get_system_data_dirs ();
    for (i = 0; dirs[i]; i++) {
        gchar *path;
        path = g_build_path (G_DIR_SEPARATOR_S, dirs[i],
                             "backgrounds", "xfce", NULL);
        if (g_file_test (path, G_FILE_TEST_IS_DIR))
            return path;
        else
            g_free (path);
    }

    return NULL;
}

/* This function loads the user's preferences */
void
xfce_bg_load_from_preferences (XfceBG     *bg,
                               GdkMonitor *monitor) {
    XfconfChannel *channel;

    channel = xfconf_channel_get ("xfce4-desktop");
    xfce_bg_load_from_xfconf (bg, channel, monitor);

    /* Queue change to force background redraw */
    queue_changed (bg);
}

static gboolean
xfce_bg_check_property_prefix (XfconfChannel *channel,
                               gchar         *prefix) {
    gchar *property;

    property = g_strconcat (prefix, "/last-image", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    property = g_strconcat (prefix, "/image-path", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    property = g_strconcat (prefix, "/image-style", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    property = g_strconcat (prefix, "/rgba1", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    property = g_strconcat (prefix, "/rgba2", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    property = g_strconcat (prefix, "/color-style", NULL);
    if (xfconf_channel_has_property (channel, property)) {
        g_free (property);
        return TRUE;
    }
    g_free (property);

    return FALSE;
}

static gchar*
xfce_bg_get_property_prefix (XfconfChannel *channel,
                             const gchar   *monitor_name) {
    gchar *prefix;

    /* Check for workspace usage */
    prefix = g_strconcat("/backdrop/screen0/monitor", monitor_name, "/workspace0", NULL);
    if (xfce_bg_check_property_prefix (channel, prefix)) {
        return prefix;
    }
    g_free(prefix);

    /* Check for non-workspace usage */
    prefix = g_strconcat("/backdrop/screen0/monitor", monitor_name, NULL);
    if (xfce_bg_check_property_prefix (channel, prefix)) {
        return prefix;
    }
    g_free(prefix);

    /* Check defaults */
    prefix = g_strdup("/backdrop/screen0/monitor0/workspace0");
    if (xfce_bg_check_property_prefix (channel, prefix)) {
        return prefix;
    }
    g_free(prefix);

    prefix = g_strdup("/backdrop/screen0/monitor0");
    return prefix;
}

static void
xfce_bg_load_from_xfconf (XfceBG        *bg,
                          XfconfChannel *channel,
                          GdkMonitor    *monitor) {
    char            *tmp;
    char            *filename;
    XfceBGColorType  ctype;
    GdkRGBA          c1, c2;
    XfceBGPlacement  placement;
    GdkMonitor      *mon;
    const gchar     *monitor_name;
    gchar           *prop_prefix;
    gchar           *property, *property2;

    g_return_if_fail(XFCE_IS_BG(bg));

    if (monitor == NULL) {
        GdkDisplay *display = gdk_display_get_default();
        mon = gdk_display_get_primary_monitor(display);
        if (mon == NULL) {
            mon = gdk_display_get_monitor(display, 0);
        }
    } else {
        mon = monitor;
    }

    monitor_name = gdk_monitor_get_model(mon);
    prop_prefix = xfce_bg_get_property_prefix (channel, monitor_name);

    property = g_strconcat(prop_prefix, "/image-style", NULL);
    placement = xfconf_channel_get_int(channel, property, XFCE_BG_PLACEMENT_NONE);
    bg->is_enabled = placement != XFCE_BG_PLACEMENT_NONE;

    /* Filename */
    g_free(property);
    property = g_strconcat(prop_prefix, "/last-image", NULL);
    if (!xfconf_channel_has_property (channel, property)) {
        g_free(property);
        property = g_strconcat(prop_prefix, "/image-path", NULL);
    }
    filename = NULL;
    tmp = xfconf_channel_get_string(channel, property,
                                    g_build_filename(find_system_backgrounds(),
                                                     XFCE_BG_FALLBACK_IMG,
                                                     NULL));
    if (tmp && *tmp != '\0') {
        /* FIXME: UTF-8 checks should go away.
         * picture-filename is of type string, which can only be used for
         * UTF-8 strings, and some filenames are not, dependending on the
         * locale used.
         * It would be better (and simpler) to change to a URI instead,
         * as URIs are UTF-8 encoded strings.
         */
        if (g_utf8_validate (tmp, -1, NULL) &&
            g_file_test (tmp, G_FILE_TEST_EXISTS)) {
            filename = g_strdup (tmp);
        } else {
            filename = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
        }

        /* Fallback to default BG if the filename set is non-existent */
        if (filename != NULL && !g_file_test (filename, G_FILE_TEST_EXISTS)) {
            g_free (filename);
            filename = g_strdup(XFCE_BG_FALLBACK_IMG);

            //* Check if default background exists, also */
            if (filename != NULL && !g_file_test (filename, G_FILE_TEST_EXISTS)) {
                g_free (filename);
                filename = NULL;
            }
        }
    }
    g_free (tmp);

    /* Colors */
    g_free(property);
    property = g_strconcat(prop_prefix, "/rgba1", NULL);
    property2 = g_strconcat(prop_prefix, "/color1", NULL);
    if (!xfconf_channel_has_property (channel, property) && xfconf_channel_has_property (channel, property2)) {
        // Using GdkColor properties from Xfdesktop 4.12
        g_free(property);
        g_free(property2);

        property = g_strconcat(prop_prefix, "/color1", NULL);
        color_from_color_array(channel, property, &c1);

        g_free(property);
        property = g_strconcat(prop_prefix, "/color2", NULL);
        color_from_color_array(channel, property, &c2);
    } else {
        // Using GdkRGBA properties from Xfdesktop 4.13
        g_free(property);
        g_free(property2);

        property = g_strconcat(prop_prefix, "/rgba1", NULL);
        color_from_rgba_array(channel, property, &c1);

        g_free(property);
        property = g_strconcat(prop_prefix, "/rgba2", NULL);
        color_from_rgba_array(channel, property, &c2);
    }

    /* Color type */
    g_free(property);
    property = g_strconcat(prop_prefix, "/color-style", NULL);
    ctype = xfconf_channel_get_int(channel, property, XFCE_BG_COLOR_SOLID);

    g_free(property);

    xfce_bg_set_color (bg, ctype, &c1, &c2);
    xfce_bg_set_placement (bg, placement);
    xfce_bg_set_filename (bg, filename);

    if (filename != NULL)
        g_free (filename);
}

static void
xfce_bg_init (XfceBG *bg) {
}

static void
xfce_bg_dispose (GObject *object) {
    XfceBG *bg = XFCE_BG (object);

    if (bg->file_monitor) {
        g_object_unref (bg->file_monitor);
        bg->file_monitor = NULL;
    }

    clear_cache (bg);

    G_OBJECT_CLASS (xfce_bg_parent_class)->dispose (object);
}

static void
xfce_bg_finalize (GObject *object) {
    XfceBG *bg = XFCE_BG (object);

    if (bg->changed_id != 0) {
        g_source_remove (bg->changed_id);
        bg->changed_id = 0;
    }

    if (bg->transitioned_id != 0) {
        g_source_remove (bg->transitioned_id);
        bg->transitioned_id = 0;
    }

    if (bg->blow_caches_id != 0) {
        g_source_remove (bg->blow_caches_id);
        bg->blow_caches_id = 0;
    }

    g_free (bg->filename);
    bg->filename = NULL;

    G_OBJECT_CLASS (xfce_bg_parent_class)->finalize (object);
}

static void
xfce_bg_class_init (XfceBGClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = xfce_bg_dispose;
    object_class->finalize = xfce_bg_finalize;

    signals[CHANGED] = g_signal_new ("changed",
                                     G_OBJECT_CLASS_TYPE (object_class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

    signals[TRANSITIONED] = g_signal_new ("transitioned",
                                          G_OBJECT_CLASS_TYPE (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);
}

XfceBG *
xfce_bg_new (void) {
    return g_object_new (XFCE_TYPE_BG, NULL);
}

static void
xfce_bg_set_color (XfceBG          *bg,
                   XfceBGColorType  type,
                   GdkRGBA         *primary,
                   GdkRGBA         *secondary) {
    g_return_if_fail (bg != NULL);
    g_return_if_fail (primary != NULL);

    if (bg->color_type != type ||
            !gdk_rgba_equal (&bg->primary, primary) ||
            (secondary && !gdk_rgba_equal (&bg->secondary, secondary))) {
        bg->color_type = type;
        bg->primary = *primary;
        if (secondary) {
            bg->secondary = *secondary;
        }

        queue_changed (bg);
    }
}

static void
xfce_bg_set_placement (XfceBG          *bg,
                       XfceBGPlacement  placement) {
    g_return_if_fail (bg != NULL);

    if (bg->placement != placement) {
        bg->placement = placement;

        queue_changed (bg);
    }
}

static inline gchar *
get_wallpaper_cache_dir () {
    return g_build_filename (g_get_user_cache_dir(), XFCE_BG_CACHE_DIR, NULL);
}

static inline gchar *
get_wallpaper_cache_prefix_name (gint            num_monitor,
                                 XfceBGPlacement placement,
                                 gint            width,
                                 gint            height) {
    return g_strdup_printf ("%i_%i_%i_%i", num_monitor, (gint) placement, width, height);
}

static char *
get_wallpaper_cache_filename (const char      *filename,
                              gint             num_monitor,
                              XfceBGPlacement  placement,
                              gint             width,
                              gint             height) {
    gchar *cache_filename;
    gchar *cache_prefix_name;
    gchar *md5_filename;
    gchar *cache_basename;
    gchar *cache_dir;

    md5_filename = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) filename,
                                                strlen (filename));
    cache_prefix_name = get_wallpaper_cache_prefix_name (num_monitor, placement, width, height);
    cache_basename = g_strdup_printf ("%s_%s", cache_prefix_name, md5_filename);
    cache_dir = get_wallpaper_cache_dir ();
    cache_filename = g_build_filename (cache_dir, cache_basename, NULL);

    g_free (cache_prefix_name);
    g_free (md5_filename);
    g_free (cache_basename);
    g_free (cache_dir);

    return cache_filename;
}

static void
cleanup_cache_for_monitor (gchar *cache_dir,
                           gint   num_monitor) {
    GDir        *g_cache_dir;
    gchar       *monitor_prefix;
    const gchar *file;

    g_cache_dir = g_dir_open (cache_dir, 0, NULL);
    monitor_prefix = g_strdup_printf ("%i_", num_monitor);

    file = g_dir_read_name (g_cache_dir);
    while (file != NULL) {
        gchar *path = g_build_filename (cache_dir, file, NULL);

        /* purge files with same monitor id */
        if (g_str_has_prefix (file, monitor_prefix) &&
            g_file_test (path, G_FILE_TEST_IS_REGULAR))
            g_unlink (path);

        g_free (path);

        file = g_dir_read_name (g_cache_dir);
    }

    g_free (monitor_prefix);
    g_dir_close (g_cache_dir);
}

static gboolean
cache_file_is_valid (const char *filename,
                     const char *cache_filename) {
    time_t mtime;
    time_t cache_mtime;

    if (!g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR))
        return FALSE;

    mtime = get_mtime (filename);
    cache_mtime = get_mtime (cache_filename);

    return (mtime < cache_mtime);
}

static void
refresh_cache_file (XfceBG    *bg,
                    GdkPixbuf *new_pixbuf,
                    gint       num_monitor,
                    gint       width,
                    gint       height) {
    gchar           *cache_filename;
    gchar           *cache_dir;

    if ((num_monitor == -1) || (width <= 300) || (height <= 300))
        return;

    cache_filename = get_wallpaper_cache_filename (bg->filename, num_monitor,
                            bg->placement, width, height);
    cache_dir = get_wallpaper_cache_dir ();

    /* Only refresh scaled file on disk if useful (and don't cache slideshow) */
    if (!cache_file_is_valid (bg->filename, cache_filename)) {
        GdkPixbufFormat *format;

        format = gdk_pixbuf_get_file_info (bg->filename, NULL, NULL);

        if (format != NULL) {
            gchar *format_name;

            if (!g_file_test (cache_dir, G_FILE_TEST_IS_DIR)) {
                g_mkdir_with_parents (cache_dir, 0700);
            } else {
                cleanup_cache_for_monitor (cache_dir, num_monitor);
            }

            format_name = gdk_pixbuf_format_get_name (format);

            if (strcmp (format_name, "jpeg") == 0)
                gdk_pixbuf_save (new_pixbuf, cache_filename, format_name,
                         NULL, "quality", "100", NULL);
            else
                gdk_pixbuf_save (new_pixbuf, cache_filename, format_name,
                         NULL, NULL);

            g_free (format_name);
        }
    }

    g_free (cache_filename);
    g_free (cache_dir);
}

static void
file_changed (GFileMonitor      *file_monitor,
              GFile             *child,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              gpointer           user_data) {
    XfceBG *bg = XFCE_BG (user_data);

    clear_cache (bg);
    queue_changed (bg);
}

static void
xfce_bg_set_filename (XfceBG     *bg,
                      const char *filename) {
    g_return_if_fail (bg != NULL);

    if (is_different (bg, filename)) {
        g_free (bg->filename);

        bg->filename = g_strdup (filename);
        bg->file_mtime = get_mtime (bg->filename);

        if (bg->file_monitor) {
            g_object_unref (bg->file_monitor);
            bg->file_monitor = NULL;
        }

        if (bg->filename) {
            GFile *f = g_file_new_for_path (bg->filename);

            bg->file_monitor = g_file_monitor_file (f, 0, NULL, NULL);
            g_signal_connect (bg->file_monitor, "changed",
                      G_CALLBACK (file_changed), bg);

            g_object_unref (f);
        }

        clear_cache (bg);

        queue_changed (bg);
    }
}

static void
draw_color_area (XfceBG       *bg,
                 GdkPixbuf    *dest,
                 GdkRectangle *rect) {
    guint32 pixel;
    GdkRectangle extent;

    extent.x = 0;
    extent.y = 0;
    extent.width = gdk_pixbuf_get_width (dest);
    extent.height = gdk_pixbuf_get_height (dest);

    gdk_rectangle_intersect (rect, &extent, rect);

    switch (bg->color_type) {
        case XFCE_BG_COLOR_SOLID:
            /* not really a big deal to ignore the area of interest */
            pixel = ((guint) (bg->primary.red * 0xff) << 24)   |
                ((guint) (bg->primary.green * 0xff) << 16) |
                ((guint) (bg->primary.blue * 0xff) << 8)   |
                (0xff);

            gdk_pixbuf_fill (dest, pixel);
            break;

        case XFCE_BG_COLOR_H_GRADIENT:
            pixbuf_draw_gradient (dest, TRUE, &(bg->primary), &(bg->secondary), rect);
            break;

        case XFCE_BG_COLOR_V_GRADIENT:
            pixbuf_draw_gradient (dest, FALSE, &(bg->primary), &(bg->secondary), rect);
            break;

        default:
            break;
    }
}

static void
draw_color (XfceBG    *bg,
            GdkPixbuf *dest) {
    GdkRectangle rect;

    rect.x = 0;
    rect.y = 0;
    rect.width = gdk_pixbuf_get_width (dest);
    rect.height = gdk_pixbuf_get_height (dest);
    draw_color_area (bg, dest, &rect);
}

static GdkPixbuf *
pixbuf_clip_to_fit (GdkPixbuf *src,
                    int        max_width,
                    int        max_height) {
    int src_width, src_height;
    int w, h;
    int src_x, src_y;
    GdkPixbuf *pixbuf;

    src_width = gdk_pixbuf_get_width (src);
    src_height = gdk_pixbuf_get_height (src);

    if (src_width < max_width && src_height < max_height)
        return g_object_ref (src);

    w = MIN(src_width, max_width);
    h = MIN(src_height, max_height);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                             gdk_pixbuf_get_has_alpha (src),
                             8, w, h);

    src_x = (src_width - w) / 2;
    src_y = (src_height - h) / 2;
    gdk_pixbuf_copy_area (src,
                          src_x, src_y,
                          w, h,
                          pixbuf,
                          0, 0);
    return pixbuf;
}

static GdkPixbuf *
get_scaled_pixbuf (XfceBGPlacement  placement,
                   GdkPixbuf       *pixbuf,
                   int              width,
                   int              height,
                   int             *x,
                   int             *y,
                   int             *w,
                   int             *h) {
    GdkPixbuf *new;

#if 0
    g_print ("original_width: %d %d\n",
         gdk_pixbuf_get_width (pixbuf),
         gdk_pixbuf_get_height (pixbuf));
#endif

    switch (placement) {
        case XFCE_BG_PLACEMENT_SPANNED:
            new = pixbuf_scale_to_fit (pixbuf, width, height);
            break;
        case XFCE_BG_PLACEMENT_ZOOMED:
            new = pixbuf_scale_to_min (pixbuf, width, height);
            break;

        case XFCE_BG_PLACEMENT_FILL_SCREEN:
            new = gdk_pixbuf_scale_simple (pixbuf, width, height,
                            GDK_INTERP_BILINEAR);
            break;

        case XFCE_BG_PLACEMENT_SCALED:
            new = pixbuf_scale_to_fit (pixbuf, width, height);
            break;

        case XFCE_BG_PLACEMENT_CENTERED:
        case XFCE_BG_PLACEMENT_TILED:
        default:
            new = pixbuf_clip_to_fit (pixbuf, width, height);
            break;
    }

    *w = gdk_pixbuf_get_width (new);
    *h = gdk_pixbuf_get_height (new);
    *x = (width - *w) / 2;
    *y = (height - *h) / 2;

    return new;
}


static void
draw_image_area (XfceBG       *bg,
                 gint          num_monitor,
                 GdkPixbuf    *pixbuf,
                 GdkPixbuf    *dest,
                 GdkRectangle *area) {
    int dest_width = area->width;
    int dest_height = area->height;
    int x, y, w, h;
    GdkPixbuf *scaled;

    if (!pixbuf)
        return;

    scaled = get_scaled_pixbuf (bg->placement, pixbuf, dest_width, dest_height, &x, &y, &w, &h);

    switch (bg->placement) {
        case XFCE_BG_PLACEMENT_TILED:
            pixbuf_tile (scaled, dest);
            break;
        case XFCE_BG_PLACEMENT_ZOOMED:
        case XFCE_BG_PLACEMENT_CENTERED:
        case XFCE_BG_PLACEMENT_FILL_SCREEN:
        case XFCE_BG_PLACEMENT_SCALED:
            pixbuf_blend (scaled, dest, 0, 0, w, h, x + area->x, y + area->y, 1.0);
            break;
        case XFCE_BG_PLACEMENT_SPANNED:
            pixbuf_blend (scaled, dest, 0, 0, w, h, x, y, 1.0);
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    refresh_cache_file (bg, scaled, num_monitor, dest_width, dest_height);

    g_object_unref (scaled);
}

static void
draw_once (XfceBG    *bg,
           GdkPixbuf *dest) {
    GdkRectangle  rect;
    GdkPixbuf    *pixbuf;
    gint          monitor;

    monitor = -1;

    rect.x = 0;
    rect.y = 0;
    rect.width = gdk_pixbuf_get_width (dest);
    rect.height = gdk_pixbuf_get_height (dest);

    pixbuf = get_pixbuf_for_size (bg, monitor, rect.width, rect.height);
    if (pixbuf) {
        draw_image_area (bg, monitor, pixbuf, dest, &rect);

        g_object_unref (pixbuf);
    }
}

static void
xfce_bg_draw (XfceBG    *bg,
              GdkPixbuf *dest) {
    if (!bg)
        return;

    draw_color (bg, dest);
    if (bg->filename && bg->placement != XFCE_BG_PLACEMENT_NONE) {
        draw_once (bg, dest);
    }
}

GdkPixbuf *
xfce_bg_get_pixbuf(XfceBG *bg,
                   int     screen_width,
                   int     screen_height,
                   int     monitor_width,
                   int     monitor_height,
                   int     scale) {
    GdkPixbuf *pixbuf;
    gint       width;
    gint       height;

    if (bg->placement == XFCE_BG_PLACEMENT_SPANNED) {
        width = screen_width;
        height = screen_height;
    } else {
        width = monitor_width;
        height = monitor_height;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                            width, height);

    xfce_bg_draw(bg, pixbuf);

    return pixbuf;
}

/* Implementation of the pixbuf cache */
struct _SlideShow {
    gint      ref_count;
    double    start_time;
    double    total_duration;

    GQueue   *slides;

    gboolean  has_multiple_sizes;

    /* used during parsing */
    struct    tm start_tm;
    GQueue   *stack;
};

static double
now (void) {
    GDateTime *tv;
    gdouble tv_sec;
    gint tv_usec;

    tv = g_date_time_new_now_local ();
    tv_sec = g_date_time_to_unix (tv);
    tv_usec = g_date_time_get_microsecond (tv);

    g_date_time_unref (tv);

    return (double)tv_sec + (tv_usec / 1000000.0);
}

static Slide *
get_current_slide (SlideShow *show,
                   double    *alpha) {
    double  delta = fmod (now() - show->start_time, show->total_duration);
    GList  *list;
    double  elapsed;

    if (delta < 0)
        delta += show->total_duration;

    elapsed = 0;
    for (list = show->slides->head; list != NULL; list = list->next) {
        Slide *slide = list->data;

        if (elapsed + slide->duration > delta) {
            if (alpha)
                *alpha = (delta - elapsed) / (double)slide->duration;
            return slide;
        }

        elapsed += slide->duration;
    }

    /* this should never happen since we have slides and we should always
     * find a current slide for the elapsed time since beginning -- we're
     * looping with fmod() */
    g_assert_not_reached ();

    return NULL;
}

static GdkPixbuf *
blend (GdkPixbuf *p1,
       GdkPixbuf *p2,
       double     alpha) {
    GdkPixbuf *result = gdk_pixbuf_copy (p1);
    GdkPixbuf *tmp;

    if (gdk_pixbuf_get_width (p2) != gdk_pixbuf_get_width (p1) ||
            gdk_pixbuf_get_height (p2) != gdk_pixbuf_get_height (p1)) {
        tmp = gdk_pixbuf_scale_simple (p2,
                           gdk_pixbuf_get_width (p1),
                           gdk_pixbuf_get_height (p1),
                           GDK_INTERP_BILINEAR);
    } else {
        tmp = g_object_ref (p2);
    }

    pixbuf_blend (tmp, result, 0, 0, -1, -1, 0, 0, alpha);

    g_object_unref (tmp);

    return result;
}

typedef enum {
    PIXBUF,
    SLIDESHOW,
    THUMBNAIL
} FileType;

struct FileCacheEntry {
    FileType type;
    char *filename;
    union {
        GdkPixbuf *pixbuf;
        SlideShow *slideshow;
        GdkPixbuf *thumbnail;
    } u;
};

static void
file_cache_entry_delete (FileCacheEntry *ent) {
    g_free (ent->filename);

    switch (ent->type) {
        case PIXBUF:
            g_object_unref (ent->u.pixbuf);
            break;
        case SLIDESHOW:
            slideshow_unref (ent->u.slideshow);
            break;
        case THUMBNAIL:
            g_object_unref (ent->u.thumbnail);
            break;
    }

    g_free (ent);
}

static void
bound_cache (XfceBG *bg) {
      while (g_list_length (bg->file_cache) >= CACHE_SIZE) {
          GList *last_link = g_list_last (bg->file_cache);
          FileCacheEntry *ent = last_link->data;

          file_cache_entry_delete (ent);

          bg->file_cache = g_list_delete_link (bg->file_cache, last_link);
      }
}

static const FileCacheEntry *
file_cache_lookup (XfceBG     *bg,
                   FileType    type,
                   const char *filename) {
    GList *list;

    for (list = bg->file_cache; list != NULL; list = list->next) {
        FileCacheEntry *ent = list->data;

        if (ent && ent->type == type &&
            strcmp (ent->filename, filename) == 0) {
            return ent;
        }
    }

    return NULL;
}

static FileCacheEntry *
file_cache_entry_new (XfceBG     *bg,
                      FileType    type,
                      const char *filename) {
    FileCacheEntry *ent = g_new0 (FileCacheEntry, 1);

    g_assert (!file_cache_lookup (bg, type, filename));

    ent->type = type;
    ent->filename = g_strdup (filename);

    bg->file_cache = g_list_prepend (bg->file_cache, ent);

    bound_cache (bg);

    return ent;
}

static void
file_cache_add_pixbuf (XfceBG     *bg,
                       const char *filename,
                       GdkPixbuf  *pixbuf) {
    FileCacheEntry *ent = file_cache_entry_new (bg, PIXBUF, filename);
    ent->u.pixbuf = g_object_ref (pixbuf);
}

static void
file_cache_add_slide_show (XfceBG     *bg,
                           const char *filename,
                           SlideShow  *show) {
    FileCacheEntry *ent = file_cache_entry_new (bg, SLIDESHOW, filename);
    ent->u.slideshow = slideshow_ref (show);
}

static GdkPixbuf *
load_from_cache_file (XfceBG     *bg,
                      const char *filename,
                      gint        num_monitor,
                      gint        best_width,
                      gint        best_height) {
    GdkPixbuf *pixbuf = NULL;
    gchar *cache_filename;

    cache_filename = get_wallpaper_cache_filename (filename, num_monitor, bg->placement,
                            best_width, best_height);

    if (cache_file_is_valid (filename, cache_filename))
        pixbuf = gdk_pixbuf_new_from_file (cache_filename, NULL);

    g_free (cache_filename);

    return pixbuf;
}

static GdkPixbuf *
get_as_pixbuf_for_size (XfceBG     *bg,
                        const char *filename,
                        gint        monitor,
                        gint        best_width,
                        gint        best_height) {
    const FileCacheEntry *ent;

    if ((ent = file_cache_lookup (bg, PIXBUF, filename))) {
        return g_object_ref (ent->u.pixbuf);
    } else {
        GdkPixbuf       *pixbuf = NULL;
        gchar           *tmp = NULL;
        GdkPixbuf       *tmp_pixbuf;

        /* Try to hit local cache first if relevant */
        if (monitor != -1)
            pixbuf = load_from_cache_file (bg, filename, monitor,
                            best_width, best_height);

        if (!pixbuf) {
            /* If scalable choose maximum size */
            GdkPixbufFormat *format;
            format = gdk_pixbuf_get_file_info (filename, NULL, NULL);
            if (format != NULL)
                tmp = gdk_pixbuf_format_get_name (format);

            if (g_strcmp0 (tmp, "svg") == 0 &&
                (best_width > 0 && best_height > 0) &&
                (bg->placement == XFCE_BG_PLACEMENT_FILL_SCREEN ||
                 bg->placement == XFCE_BG_PLACEMENT_SCALED ||
                 bg->placement == XFCE_BG_PLACEMENT_ZOOMED)) {
                pixbuf = gdk_pixbuf_new_from_file_at_size (filename,
                                       best_width,
                                       best_height, NULL);
            } else {
                pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
            }

            if (tmp != NULL)
                g_free (tmp);
        }

        if (pixbuf) {
            tmp_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
            g_object_unref (pixbuf);
            pixbuf = tmp_pixbuf;
            file_cache_add_pixbuf (bg, filename, pixbuf);
        }

        return pixbuf;
    }
}

static SlideShow *
get_as_slideshow (XfceBG     *bg,
                  const char *filename) {
    const FileCacheEntry *ent;
    if ((ent = file_cache_lookup (bg, SLIDESHOW, filename))) {
        return slideshow_ref (ent->u.slideshow);
    } else {
        SlideShow *show = read_slideshow_file (filename, NULL);

        if (show)
            file_cache_add_slide_show (bg, filename, show);

        return show;
    }
}

static gboolean
blow_expensive_caches (gpointer data) {
    XfceBG *bg = data;
    GList  *list;

    bg->blow_caches_id = 0;

    if (bg->file_cache) {
        for (list = bg->file_cache; list != NULL; list = list->next) {
            FileCacheEntry *ent = list->data;

            if (ent->type == PIXBUF) {
                file_cache_entry_delete (ent);
                bg->file_cache = g_list_delete_link (bg->file_cache,
                                     list);
            }
        }
    }

    if (bg->pixbuf_cache) {
        g_object_unref (bg->pixbuf_cache);
        bg->pixbuf_cache = NULL;
    }

    return FALSE;
}

static void
blow_expensive_caches_in_idle (XfceBG *bg) {
    if (bg->blow_caches_id == 0) {
        bg->blow_caches_id =
            g_idle_add (blow_expensive_caches,
                    bg);
    }
}


static gboolean
on_timeout (gpointer data) {
    XfceBG *bg = data;

    bg->timeout_id = 0;

    queue_transitioned (bg);

    return FALSE;
}

static double
get_slide_timeout (Slide *slide) {
    double timeout;
    if (slide->fixed) {
        timeout = slide->duration;
    } else {
        /* Maybe the number of steps should be configurable? */

        /* In the worst case we will do a fade from 0 to 256, which mean
         * we will never use more than 255 steps, however in most cases
         * the first and last value are similar and users can't percieve
         * changes in pixel values as small as 1/255th. So, lets not waste
         * CPU cycles on transitioning to often.
         *
         * 64 steps is enough for each step to be just detectable in a 16bit
         * color mode in the worst case, so we'll use this as an approximation
         * of whats detectable.
         */
        timeout = slide->duration / 64.0;
    }
    return timeout;
}

static void
ensure_timeout (XfceBG *bg,
                Slide  *slide) {
    if (!bg->timeout_id) {
        double timeout = get_slide_timeout (slide);

        /* G_MAXUINT means "only one slide" */
        if (timeout < G_MAXUINT) {
            bg->timeout_id = g_timeout_add_full (
                G_PRIORITY_LOW,
                timeout * 1000, on_timeout, bg, NULL);
        }
    }
}

static time_t
get_mtime (const char *filename) {
    time_t mtime;

    mtime = (time_t)-1;

    if (filename) {
        GFile     *file;
        GFileInfo *info;
        file = g_file_new_for_path (filename);
        info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (info) {
            mtime = g_file_info_get_attribute_uint64 (info,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED);
            g_object_unref (info);
        }
        g_object_unref (file);
    }

    return mtime;
}

/*
 * Find the FileSize that best matches the given size.
 * Do two passes; the first pass only considers FileSizes
 * that are larger than the given size.
 * We are looking for the image that best matches the aspect ratio.
 * When two images have the same aspect ratio, prefer the one whose
 * width is closer to the given width.
 */
static FileSize *
find_best_size (GSList *sizes,
                gint    width,
                gint    height) {
    GSList   *s;
    gdouble   a, d, distance;
    FileSize *best = NULL;
    gint      pass;

    a = width/(gdouble)height;
    distance = 10000.0;

    for (pass = 0; pass < 2; pass++) {
        for (s = sizes; s; s = s->next) {
            FileSize *size = s->data;

            if (pass == 0 && (size->width < width || size->height < height))
                continue;

            d = fabs (a - size->width/(gdouble)size->height);
            if (d < distance) {
                distance = d;
                best = size;
            } else if (d == distance && best != NULL) {
                if (abs (size->width - width) < abs (best->width - width)) {
                    best = size;
                }
            }
        }

        if (best)
            break;
    }

    return best;
}

static GdkPixbuf *
get_pixbuf_for_size (XfceBG *bg,
                     gint    monitor,
                     gint    best_width,
                     gint    best_height) {
    guint time_until_next_change;
    gboolean hit_cache = FALSE;

    /* only hit the cache if the aspect ratio matches */
    if (bg->pixbuf_cache) {
        int width, height;
        width = gdk_pixbuf_get_width (bg->pixbuf_cache);
        height = gdk_pixbuf_get_height (bg->pixbuf_cache);
        hit_cache = 0.2 > fabs ((best_width / (double)best_height) - (width / (double)height));
        if (!hit_cache) {
            g_object_unref (bg->pixbuf_cache);
            bg->pixbuf_cache = NULL;
        }
    }

    if (!hit_cache && bg->filename) {
        bg->file_mtime = get_mtime (bg->filename);

        bg->pixbuf_cache = get_as_pixbuf_for_size (bg, bg->filename, monitor,
                               best_width, best_height);
        time_until_next_change = G_MAXUINT;
        if (!bg->pixbuf_cache) {
            SlideShow *show = get_as_slideshow (bg, bg->filename);

            if (show) {
                double alpha;
                Slide *slide;

                slideshow_ref (show);

                slide = get_current_slide (show, &alpha);
                time_until_next_change = (guint)get_slide_timeout (slide);
                if (slide->fixed) {
                    FileSize *size = find_best_size (slide->file1,
                                     best_width, best_height);
                    bg->pixbuf_cache =
                        get_as_pixbuf_for_size (bg, size->file, monitor,
                                    best_width, best_height);
                } else {
                    FileSize *size;
                    GdkPixbuf *p1, *p2;

                    size = find_best_size (slide->file1,
                                best_width, best_height);
                    p1 = get_as_pixbuf_for_size (bg, size->file, monitor,
                                     best_width, best_height);

                    size = find_best_size (slide->file2,
                                best_width, best_height);
                    p2 = get_as_pixbuf_for_size (bg, size->file, monitor,
                                     best_width, best_height);

                    if (p1 && p2)
                        bg->pixbuf_cache = blend (p1, p2, alpha);
                    if (p1)
                        g_object_unref (p1);
                    if (p2)
                        g_object_unref (p2);
                }

                ensure_timeout (bg, slide);

                slideshow_unref (show);
            }
        }

        /* If the next slideshow step is a long time away then
           we blow away the expensive stuff (large pixbufs) from
           the cache */
        if (time_until_next_change > KEEP_EXPENSIVE_CACHE_SECS)
            blow_expensive_caches_in_idle (bg);
    }

    if (bg->pixbuf_cache)
        g_object_ref (bg->pixbuf_cache);

    return bg->pixbuf_cache;
}

static gboolean
is_different (XfceBG     *bg,
              const char *filename) {
    if (!filename && bg->filename) {
        return TRUE;
    } else if (filename && !bg->filename) {
        return TRUE;
    } else if (!filename && !bg->filename) {
        return FALSE;
    } else {
        time_t mtime = get_mtime (filename);

        if (mtime != bg->file_mtime)
            return TRUE;

        if (strcmp (filename, bg->filename) != 0)
            return TRUE;

        return FALSE;
    }
}

static void
clear_cache (XfceBG *bg) {
    GList *list;

    if (bg->file_cache) {
        for (list = bg->file_cache; list != NULL; list = list->next) {
            FileCacheEntry *ent = list->data;

            file_cache_entry_delete (ent);
        }
        g_list_free (bg->file_cache);
        bg->file_cache = NULL;
    }

    if (bg->pixbuf_cache) {
        g_object_unref (bg->pixbuf_cache);

        bg->pixbuf_cache = NULL;
    }

    if (bg->timeout_id) {
        g_source_remove (bg->timeout_id);

        bg->timeout_id = 0;
    }
}

static GdkPixbuf *
pixbuf_scale_to_fit (GdkPixbuf *src,
                     int        max_width,
                     int        max_height) {
    double factor;
    int    src_width, src_height;
    int    new_width, new_height;

    src_width = gdk_pixbuf_get_width (src);
    src_height = gdk_pixbuf_get_height (src);

    factor = MIN (max_width  / (double) src_width, max_height / (double) src_height);

    new_width  = floor (src_width * factor + 0.5);
    new_height = floor (src_height * factor + 0.5);

    return gdk_pixbuf_scale_simple (src, new_width, new_height, GDK_INTERP_BILINEAR);
}

static GdkPixbuf *
pixbuf_scale_to_min (GdkPixbuf *src,
                     int        min_width,
                     int        min_height) {
    double     factor;
    int        src_width, src_height;
    int        new_width, new_height;
    GdkPixbuf *dest;

    src_width = gdk_pixbuf_get_width (src);
    src_height = gdk_pixbuf_get_height (src);

    factor = MAX (min_width / (double) src_width, min_height / (double) src_height);

    new_width = floor (src_width * factor + 0.5);
    new_height = floor (src_height * factor + 0.5);

    dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                   gdk_pixbuf_get_has_alpha (src),
                   8, min_width, min_height);
    if (!dest)
        return NULL;

    /* crop the result */
    gdk_pixbuf_scale (src, dest,
                      0, 0,
                      min_width, min_height,
                      (new_width - min_width) / -2,
                      (new_height - min_height) / -2,
                      factor,
                      factor,
                      GDK_INTERP_BILINEAR);
    return dest;
}

static guchar *
create_gradient (const GdkRGBA *primary,
                 const GdkRGBA *secondary,
                 int            n_pixels) {
    guchar *result = g_malloc (n_pixels * 3);
    int     i;

    for (i = 0; i < n_pixels; ++i) {
        double ratio = (i + 0.5) / n_pixels;

        result[3 * i + 0] = (guchar) ((primary->red * (1 - ratio) + secondary->red * ratio) * 0x100);
        result[3 * i + 1] = (guchar) ((primary->green * (1 - ratio) + secondary->green * ratio) * 0x100);
        result[3 * i + 2] = (guchar) ((primary->blue * (1 - ratio) + secondary->blue * ratio) * 0x100);
    }

    return result;
}

static void
pixbuf_draw_gradient (GdkPixbuf    *pixbuf,
                      gboolean      horizontal,
                      GdkRGBA      *c1,
                      GdkRGBA      *c2,
                      GdkRectangle *rect) {
    int     width;
    int     height;
    int     rowstride;
    guchar *dst;
    int     n_channels = 3;

    rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    width = rect->width;
    height = rect->height;
    dst = gdk_pixbuf_get_pixels (pixbuf) + rect->x * n_channels + rowstride * rect->y;

    if (horizontal) {
        guchar *gradient = create_gradient (c1, c2, width);
        int copy_bytes_per_row = width * n_channels;
        int i;

        for (i = 0; i < height; i++) {
            guchar *d;
            d = dst + rowstride * i;
            memcpy (d, gradient, copy_bytes_per_row);
        }
        g_free (gradient);
    } else {
        guchar *gb, *gradient;
        int i;

        gradient = create_gradient (c1, c2, height);
        for (i = 0; i < height; i++) {
            int j;
            guchar *d;

            d = dst + rowstride * i;
            gb = gradient + n_channels * i;
            for (j = width; j > 0; j--) {
                int k;

                for (k = 0; k < n_channels; k++) {
                    *(d++) = gb[k];
                }
            }
        }

        g_free (gradient);
    }
}

static void
pixbuf_blend (GdkPixbuf *src,
              GdkPixbuf *dest,
              int        src_x,
              int        src_y,
              int        src_width,
              int        src_height,
              int        dest_x,
              int        dest_y,
              double     alpha) {
    int dest_width = gdk_pixbuf_get_width (dest);
    int dest_height = gdk_pixbuf_get_height (dest);
    int offset_x = dest_x - src_x;
    int offset_y = dest_y - src_y;

    if (src_width < 0)
        src_width = gdk_pixbuf_get_width (src);

    if (src_height < 0)
        src_height = gdk_pixbuf_get_height (src);

    if (dest_x < 0)
        dest_x = 0;

    if (dest_y < 0)
        dest_y = 0;

    if (dest_x + src_width > dest_width) {
        src_width = dest_width - dest_x;
    }

    if (dest_y + src_height > dest_height) {
        src_height = dest_height - dest_y;
    }

    gdk_pixbuf_composite (src, dest,
                          dest_x, dest_y,
                          src_width, src_height,
                          offset_x, offset_y,
                          1, 1, GDK_INTERP_NEAREST,
                          alpha * 0xFF + 0.5);
}

static void
pixbuf_tile (GdkPixbuf *src,
             GdkPixbuf *dest) {
    int x, y;
    int tile_width, tile_height;
    int dest_width = gdk_pixbuf_get_width (dest);
    int dest_height = gdk_pixbuf_get_height (dest);

    tile_width = gdk_pixbuf_get_width (src);
    tile_height = gdk_pixbuf_get_height (src);

    for (y = 0; y < dest_height; y += tile_height) {
        for (x = 0; x < dest_width; x += tile_width) {
            pixbuf_blend (src, dest, 0, 0,
                      tile_width, tile_height, x, y, 1.0);
        }
    }
}

static gboolean stack_is (SlideShow *parser,
                          const char *s1,
                          ...);

/* Parser for fading background */
static void
handle_start_element (GMarkupParseContext  *context,
                      const gchar          *name,
                      const gchar         **attr_names,
                      const gchar         **attr_values,
                      gpointer              user_data,
                      GError              **err) {
    SlideShow *parser = user_data;
    gint       i;

    if (strcmp (name, "static") == 0 || strcmp (name, "transition") == 0) {
        Slide *slide = g_new0 (Slide, 1);

        if (strcmp (name, "static") == 0)
            slide->fixed = TRUE;

        g_queue_push_tail (parser->slides, slide);
    } else if (strcmp (name, "size") == 0) {
        Slide *slide = parser->slides->tail->data;
        FileSize *size = g_new0 (FileSize, 1);
        for (i = 0; attr_names[i]; i++) {
            if (strcmp (attr_names[i], "width") == 0)
                size->width = atoi (attr_values[i]);
            else if (strcmp (attr_names[i], "height") == 0)
                size->height = atoi (attr_values[i]);
        }
        if (parser->stack->tail &&
            (strcmp (parser->stack->tail->data, "file") == 0 ||
             strcmp (parser->stack->tail->data, "from") == 0)) {
            slide->file1 = g_slist_prepend (slide->file1, size);
        } else if (parser->stack->tail &&
             strcmp (parser->stack->tail->data, "to") == 0) {
            slide->file2 = g_slist_prepend (slide->file2, size);
        }
    }
    g_queue_push_tail (parser->stack, g_strdup (name));
}

static void
handle_end_element (GMarkupParseContext  *context,
                    const gchar          *name,
                    gpointer              user_data,
                    GError              **err) {
    SlideShow *parser = user_data;

    g_free (g_queue_pop_tail (parser->stack));
}

static gboolean
stack_is (SlideShow  *parser,
          const char *s1,
          ...) {
    GList      *stack = NULL;
    const char *s;
    GList      *l1, *l2;
    va_list     args;

    stack = g_list_prepend (stack, (gpointer)s1);

    va_start (args, s1);

    s = va_arg (args, const char *);
    while (s) {
        stack = g_list_prepend (stack, (gpointer)s);
        s = va_arg (args, const char *);
    }

    va_end (args);

    l1 = stack;
    l2 = parser->stack->head;

    while (l1 && l2) {
        if (strcmp (l1->data, l2->data) != 0) {
            g_list_free (stack);
            return FALSE;
        }

        l1 = l1->next;
        l2 = l2->next;
    }

    g_list_free (stack);

    return (!l1 && !l2);
}

static int
parse_int (const char *text) {
    return strtol (text, NULL, 0);
}

static void
handle_text (GMarkupParseContext  *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **err) {
    SlideShow *parser = user_data;
    FileSize  *fs;
    gint       i;

    g_return_if_fail (parser != NULL);
    g_return_if_fail (parser->slides != NULL);

    Slide *slide = parser->slides->tail ? parser->slides->tail->data : NULL;

    if (stack_is (parser, "year", "starttime", "background", NULL)) {
        parser->start_tm.tm_year = parse_int (text) - 1900;
    } else if (stack_is (parser, "month", "starttime", "background", NULL)) {
        parser->start_tm.tm_mon = parse_int (text) - 1;
    } else if (stack_is (parser, "day", "starttime", "background", NULL)) {
        parser->start_tm.tm_mday = parse_int (text);
    } else if (stack_is (parser, "hour", "starttime", "background", NULL)) {
        parser->start_tm.tm_hour = parse_int (text) - 1;
    } else if (stack_is (parser, "minute", "starttime", "background", NULL)) {
        parser->start_tm.tm_min = parse_int (text);
    } else if (stack_is (parser, "second", "starttime", "background", NULL)) {
        parser->start_tm.tm_sec = parse_int (text);
    } else if (stack_is (parser, "duration", "static", "background", NULL) ||
         stack_is (parser, "duration", "transition", "background", NULL)) {
        g_return_if_fail (slide != NULL);

        slide->duration = g_strtod (text, NULL);
        parser->total_duration += slide->duration;
    } else if (stack_is (parser, "file", "static", "background", NULL) ||
         stack_is (parser, "from", "transition", "background", NULL)) {
        g_return_if_fail (slide != NULL);

        for (i = 0; text[i]; i++) {
            if (!g_ascii_isspace (text[i]))
                break;
        }
        if (text[i] == 0)
            return;
        fs = g_new (FileSize, 1);
        fs->width = -1;
        fs->height = -1;
        fs->file = g_strdup (text);
        slide->file1 = g_slist_prepend (slide->file1, fs);
        if (slide->file1->next != NULL)
            parser->has_multiple_sizes = TRUE;
    } else if (stack_is (parser, "size", "file", "static", "background", NULL) ||
         stack_is (parser, "size", "from", "transition", "background", NULL)) {
        g_return_if_fail (slide != NULL);

        fs = slide->file1->data;
        fs->file = g_strdup (text);
        if (slide->file1->next != NULL)
            parser->has_multiple_sizes = TRUE;
    } else if (stack_is (parser, "to", "transition", "background", NULL)) {
        g_return_if_fail (slide != NULL);

        for (i = 0; text[i]; i++) {
            if (!g_ascii_isspace (text[i]))
                break;
        }
        if (text[i] == 0)
            return;
        fs = g_new (FileSize, 1);
        fs->width = -1;
        fs->height = -1;
        fs->file = g_strdup (text);
        slide->file2 = g_slist_prepend (slide->file2, fs);
        if (slide->file2->next != NULL)
            parser->has_multiple_sizes = TRUE;
    } else if (stack_is (parser, "size", "to", "transition", "background", NULL)) {
        g_return_if_fail (slide != NULL);

        fs = slide->file2->data;
        fs->file = g_strdup (text);
        if (slide->file2->next != NULL)
            parser->has_multiple_sizes = TRUE;
    }
}

static SlideShow *
slideshow_ref (SlideShow *show) {
    show->ref_count++;
    return show;
}

static void
slideshow_unref (SlideShow *show) {
    GList    *list;
    GSList   *slist;
    FileSize *size;

    show->ref_count--;
    if (show->ref_count > 0)
        return;

    for (list = show->slides->head; list != NULL; list = list->next) {
        Slide *slide = list->data;

        for (slist = slide->file1; slist != NULL; slist = slist->next) {
            size = slist->data;
            g_free (size->file);
            g_free (size);
        }
        g_slist_free (slide->file1);

        for (slist = slide->file2; slist != NULL; slist = slist->next) {
            size = slist->data;
            g_free (size->file);
            g_free (size);
        }
        g_slist_free (slide->file2);

        g_free (slide);
    }

    g_queue_free (show->slides);

    g_list_foreach (show->stack->head, (GFunc) g_free, NULL);
    g_queue_free (show->stack);

    g_free (show);
}

static void
dump_bg (SlideShow *show) {
#if 0
    GList  *list;
    GSList *slist;

    for (list = show->slides->head; list != NULL; list = list->next) {
        Slide *slide = list->data;

        g_print ("\nSlide: %s\n", slide->fixed? "fixed" : "transition");
        g_print ("duration: %f\n", slide->duration);
        g_print ("File1:\n");
        for (slist = slide->file1; slist != NULL; slist = slist->next) {
            FileSize *size = slist->data;
            g_print ("\t%s (%dx%d)\n",
                 size->file, size->width, size->height);
        }
        g_print ("File2:\n");
        for (slist = slide->file2; slist != NULL; slist = slist->next) {
            FileSize *size = slist->data;
            g_print ("\t%s (%dx%d)\n",
                 size->file, size->width, size->height);
        }
    }
#endif
}

static void
threadsafe_localtime (time_t     time,
                      struct tm *tm) {
    struct tm *res;

    G_LOCK_DEFINE_STATIC (localtime_mutex);

    G_LOCK (localtime_mutex);

    res = localtime (&time);
    if (tm) {
        *tm = *res;
    }

    G_UNLOCK (localtime_mutex);
}

static SlideShow *
read_slideshow_file (const char  *filename,
                     GError     **err) {
    GMarkupParser parser = {
        handle_start_element,
        handle_end_element,
        handle_text,
        NULL, /* passthrough */
        NULL, /* error */
    };

    GFile               *file;
    char                *contents = NULL;
    gsize                len;
    SlideShow           *show = NULL;
    GMarkupParseContext *context = NULL;

    if (!filename)
        return NULL;

    file = g_file_new_for_path (filename);
    if (!g_file_load_contents (file, NULL, &contents, &len, NULL, NULL)) {
        g_object_unref (file);
        return NULL;
    }
    g_object_unref (file);

    show = g_new0 (SlideShow, 1);
    show->ref_count = 1;
    threadsafe_localtime ((time_t)0, &show->start_tm);
    show->stack = g_queue_new ();
    show->slides = g_queue_new ();

    context = g_markup_parse_context_new (&parser, 0, show, NULL);

    if (!g_markup_parse_context_parse (context, contents, len, err)) {
        slideshow_unref (show);
        show = NULL;
    }


    if (show) {
        if (!g_markup_parse_context_end_parse (context, err)) {
            slideshow_unref (show);
            show = NULL;
        }
    }

    g_markup_parse_context_free (context);

    if (show) {
        time_t t;
        int    qlen;

        t = mktime (&show->start_tm);

        show->start_time = (double)t;

        dump_bg (show);

        qlen = g_queue_get_length (show->slides);

        /* no slides, that's not a slideshow */
        if (qlen == 0) {
            slideshow_unref (show);
            show = NULL;
        /* one slide, there's no transition */
        } else if (qlen == 1) {
            Slide *slide = show->slides->head->data;
            slide->duration = show->total_duration = G_MAXUINT;
        }
    }

    g_free (contents);

    return show;
}
