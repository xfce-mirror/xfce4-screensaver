/*
 * mate-thumbnail.c: Utilities for handling thumbnails
 *
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This file is part of the Mate Library.
 *
 * The Mate Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Mate Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Mate Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include "xfce-desktop-thumbnail.h"
#include <glib/gstdio.h>
#include <glib-unix.h>

#define SECONDS_BETWEEN_STATS 10

struct _XfceDesktopThumbnailFactoryPrivate {
  XfceDesktopThumbnailSize size;

  GMutex lock;

  GList *thumbnailers;
  GHashTable *mime_types_map;
  GList *monitors;

  GSettings *settings;
  gboolean loaded : 1;
  gboolean disabled : 1;
  gchar **disabled_types;
};

static const char *appname = "mate-thumbnail-factory";

static void xfce_desktop_thumbnail_factory_init          (XfceDesktopThumbnailFactory      *factory);
static void xfce_desktop_thumbnail_factory_class_init    (XfceDesktopThumbnailFactoryClass *class);

G_DEFINE_TYPE (XfceDesktopThumbnailFactory,
	       xfce_desktop_thumbnail_factory,
	       G_TYPE_OBJECT)
#define parent_class xfce_desktop_thumbnail_factory_parent_class

#define XFCE_DESKTOP_THUMBNAIL_FACTORY_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), XFCE_DESKTOP_TYPE_THUMBNAIL_FACTORY, XfceDesktopThumbnailFactoryPrivate))

typedef struct {
    gint width;
    gint height;
    gint input_width;
    gint input_height;
    gboolean preserve_aspect_ratio;
} SizePrepareContext;

#define LOAD_BUFFER_SIZE 4096

#define THUMBNAILER_ENTRY_GROUP "Thumbnailer Entry"
#define THUMBNAILER_EXTENSION   ".thumbnailer"

typedef struct {
    volatile gint ref_count;

    gchar *path;

    gchar  *try_exec;
    gchar  *command;
    gchar **mime_types;
} Thumbnailer;

static Thumbnailer *
thumbnailer_ref (Thumbnailer *thumb)
{
  g_return_val_if_fail (thumb != NULL, NULL);
  g_return_val_if_fail (thumb->ref_count > 0, NULL);

  g_atomic_int_inc (&thumb->ref_count);
  return thumb;
}

static void
thumbnailer_unref (Thumbnailer *thumb)
{
  g_return_if_fail (thumb != NULL);
  g_return_if_fail (thumb->ref_count > 0);

  if (g_atomic_int_dec_and_test (&thumb->ref_count))
    {
      g_free (thumb->path);
      g_free (thumb->try_exec);
      g_free (thumb->command);
      g_strfreev (thumb->mime_types);

      g_slice_free (Thumbnailer, thumb);
    }
}

static Thumbnailer *
thumbnailer_load (Thumbnailer *thumb)
{
  GKeyFile *key_file;
  GError *error = NULL;

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file, thumb->path, 0, &error))
    {
      g_warning ("Failed to load thumbnailer from \"%s\": %s\n", thumb->path, error->message);
      g_error_free (error);
      thumbnailer_unref (thumb);
      g_key_file_free (key_file);

      return NULL;
    }

  if (!g_key_file_has_group (key_file, THUMBNAILER_ENTRY_GROUP))
    {
      g_warning ("Invalid thumbnailer: missing group \"%s\"\n", THUMBNAILER_ENTRY_GROUP);
      thumbnailer_unref (thumb);
      g_key_file_free (key_file);

      return NULL;
    }

  thumb->command = g_key_file_get_string (key_file, THUMBNAILER_ENTRY_GROUP, "Exec", NULL);
  if (!thumb->command)
    {
      g_warning ("Invalid thumbnailer: missing Exec key\n");
      thumbnailer_unref (thumb);
      g_key_file_free (key_file);

      return NULL;
    }

  thumb->mime_types = g_key_file_get_string_list (key_file, THUMBNAILER_ENTRY_GROUP, "MimeType", NULL, NULL);
  if (!thumb->mime_types)
    {
      g_warning ("Invalid thumbnailer: missing MimeType key\n");
      thumbnailer_unref (thumb);
      g_key_file_free (key_file);

      return NULL;
    }

  thumb->try_exec = g_key_file_get_string (key_file, THUMBNAILER_ENTRY_GROUP, "TryExec", NULL);

  g_key_file_free (key_file);

  return thumb;
}

static Thumbnailer *
thumbnailer_reload (Thumbnailer *thumb)
{
  g_return_val_if_fail (thumb != NULL, NULL);

  g_free (thumb->command);
  thumb->command = NULL;
  g_strfreev (thumb->mime_types);
  thumb->mime_types = NULL;
  g_free (thumb->try_exec);
  thumb->try_exec = NULL;

  return thumbnailer_load (thumb);
}

static Thumbnailer *
thumbnailer_new (const gchar *path)
{
  Thumbnailer *thumb;

  thumb = g_slice_new0 (Thumbnailer);
  thumb->ref_count = 1;
  thumb->path = g_strdup (path);

  return thumbnailer_load (thumb);
}

static gboolean
thumbnailer_try_exec (Thumbnailer *thumb)
{
  gchar *path;
  gboolean retval;

  if (G_UNLIKELY (!thumb))
    return FALSE;

  /* TryExec is optinal, but Exec isn't, so we assume
   * the thumbnailer can be run when TryExec is not present
   */
  if (!thumb->try_exec)
    return TRUE;

  path = g_find_program_in_path (thumb->try_exec);
  retval = path != NULL;
  g_free (path);

  return retval;
}

static gpointer
init_thumbnailers_dirs (gpointer data)
{
  const gchar * const *data_dirs;
  gchar **thumbs_dirs;
  guint i, length;

  data_dirs = g_get_system_data_dirs ();
  length = g_strv_length ((char **) data_dirs);

  thumbs_dirs = g_new (gchar *, length + 2);
  thumbs_dirs[0] = g_build_filename (g_get_user_data_dir (), "thumbnailers", NULL);
  for (i = 0; i < length; i++)
    thumbs_dirs[i + 1] = g_build_filename (data_dirs[i], "thumbnailers", NULL);
  thumbs_dirs[length + 1] = NULL;

  return thumbs_dirs;
}

static const gchar * const *
get_thumbnailers_dirs (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, init_thumbnailers_dirs, NULL);
}

static void
xfce_desktop_thumbnail_factory_finalize (GObject *object)
{
  XfceDesktopThumbnailFactory *factory;
  XfceDesktopThumbnailFactoryPrivate *priv;
  
  factory = XFCE_DESKTOP_THUMBNAIL_FACTORY (object);

  priv = factory->priv;

  if (priv->thumbnailers)
    {
      g_list_free_full (priv->thumbnailers, (GDestroyNotify)thumbnailer_unref);
      priv->thumbnailers = NULL;
    }

  if (priv->mime_types_map)
    {
      g_hash_table_destroy (priv->mime_types_map);
      priv->mime_types_map = NULL;
    }

  if (priv->monitors)
    {
      g_list_free_full (priv->monitors, (GDestroyNotify)g_object_unref);
      priv->monitors = NULL;
    }

  g_mutex_clear (&priv->lock);

  if (priv->disabled_types)
    {
      g_strfreev (priv->disabled_types);
      priv->disabled_types = NULL;
    }

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* These should be called with the lock held */
static void
xfce_desktop_thumbnail_factory_register_mime_types (XfceDesktopThumbnailFactory *factory,
                                                     Thumbnailer                  *thumb)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  gint i;

  for (i = 0; thumb->mime_types[i]; i++)
    {
      if (!g_hash_table_lookup (priv->mime_types_map, thumb->mime_types[i]))
        g_hash_table_insert (priv->mime_types_map,
                             g_strdup (thumb->mime_types[i]),
                             thumbnailer_ref (thumb));
    }
}

static void
xfce_desktop_thumbnail_factory_add_thumbnailer (XfceDesktopThumbnailFactory *factory,
                                                 Thumbnailer                  *thumb)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;

  xfce_desktop_thumbnail_factory_register_mime_types (factory, thumb);
  priv->thumbnailers = g_list_prepend (priv->thumbnailers, thumb);
}

static gboolean
xfce_desktop_thumbnail_factory_is_disabled (XfceDesktopThumbnailFactory *factory,
                                             const gchar                  *mime_type)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  guint i;

  if (priv->disabled)
    return TRUE;

  if (!priv->disabled_types)
    return FALSE;

  for (i = 0; priv->disabled_types[i]; i++)
    {
      if (g_strcmp0 (priv->disabled_types[i], mime_type) == 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
remove_thumbnailer_from_mime_type_map (gchar       *key,
                                       Thumbnailer *value,
                                       gchar       *path)
{
  return (strcmp (value->path, path) == 0);
}


static void
update_or_create_thumbnailer (XfceDesktopThumbnailFactory *factory,
                              const gchar                  *path)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  GList *l;
  Thumbnailer *thumb;
  gboolean found = FALSE;

  g_mutex_lock (&priv->lock);

  for (l = priv->thumbnailers; l && !found; l = g_list_next (l))
    {
      thumb = (Thumbnailer *)l->data;

      if (strcmp (thumb->path, path) == 0)
        {
          found = TRUE;

          /* First remove the mime_types associated to this thumbnailer */
          g_hash_table_foreach_remove (priv->mime_types_map,
                                       (GHRFunc)remove_thumbnailer_from_mime_type_map,
                                       (gpointer)path);
          if (!thumbnailer_reload (thumb))
              priv->thumbnailers = g_list_delete_link (priv->thumbnailers, l);
          else
              xfce_desktop_thumbnail_factory_register_mime_types (factory, thumb);
        }
    }

  if (!found)
    {
      thumb = thumbnailer_new (path);
      if (thumb)
        xfce_desktop_thumbnail_factory_add_thumbnailer (factory, thumb);
    }

  g_mutex_unlock (&priv->lock);
}

static void
remove_thumbnailer (XfceDesktopThumbnailFactory *factory,
                    const gchar                  *path)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  GList *l;
  Thumbnailer *thumb;

  g_mutex_lock (&priv->lock);

  for (l = priv->thumbnailers; l; l = g_list_next (l))
    {
      thumb = (Thumbnailer *)l->data;

      if (strcmp (thumb->path, path) == 0)
        {
          priv->thumbnailers = g_list_delete_link (priv->thumbnailers, l);
          g_hash_table_foreach_remove (priv->mime_types_map,
                                       (GHRFunc)remove_thumbnailer_from_mime_type_map,
                                       (gpointer)path);
          thumbnailer_unref (thumb);

          break;
        }
    }

  g_mutex_unlock (&priv->lock);
}

static void
thumbnailers_directory_changed (GFileMonitor                 *monitor,
                                GFile                        *file,
                                GFile                        *other_file,
                                GFileMonitorEvent             event_type,
                                XfceDesktopThumbnailFactory *factory)
{
  gchar *path;

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_DELETED:
      path = g_file_get_path (file);
      if (!g_str_has_suffix (path, THUMBNAILER_EXTENSION))
        {
          g_free (path);
          return;
        }

      if (event_type == G_FILE_MONITOR_EVENT_DELETED)
        remove_thumbnailer (factory, path);
      else
        update_or_create_thumbnailer (factory, path);

      g_free (path);
      break;
    default:
      break;
    }
}

static void
xfce_desktop_thumbnail_factory_load_thumbnailers (XfceDesktopThumbnailFactory *factory)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  const gchar * const *dirs;
  guint i;

  if (priv->loaded)
    return;

  dirs = get_thumbnailers_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      const gchar *path = dirs[i];
      GDir *dir;
      GFile *dir_file;
      GFileMonitor *monitor;
      const gchar *dirent;

      dir = g_dir_open (path, 0, NULL);
      if (!dir)
        continue;

      /* Monitor dir */
      dir_file = g_file_new_for_path (path);
      monitor = g_file_monitor_directory (dir_file,
                                          G_FILE_MONITOR_NONE,
                                          NULL, NULL);
      if (monitor)
        {
          g_signal_connect (monitor, "changed",
                            G_CALLBACK (thumbnailers_directory_changed),
                            factory);
          priv->monitors = g_list_prepend (priv->monitors, monitor);
        }
      g_object_unref (dir_file);

      while ((dirent = g_dir_read_name (dir)))
        {
          Thumbnailer *thumb;
          gchar       *filename;

          if (!g_str_has_suffix (dirent, THUMBNAILER_EXTENSION))
            continue;

          filename = g_build_filename (path, dirent, NULL);
          thumb = thumbnailer_new (filename);
          g_free (filename);

          if (thumb)
            xfce_desktop_thumbnail_factory_add_thumbnailer (factory, thumb);
        }

      g_dir_close (dir);
    }

  priv->loaded = TRUE;
}

static void
external_thumbnailers_disabled_all_changed_cb (GSettings                    *settings,
                                               const gchar                  *key,
                                               XfceDesktopThumbnailFactory *factory)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;

  g_mutex_lock (&priv->lock);

  priv->disabled = g_settings_get_boolean (priv->settings, "disable-all");
  if (priv->disabled)
    {
      g_strfreev (priv->disabled_types);
      priv->disabled_types = NULL;
    }
  else
    {
      priv->disabled_types = g_settings_get_strv (priv->settings, "disable");
      xfce_desktop_thumbnail_factory_load_thumbnailers (factory);
    }

  g_mutex_unlock (&priv->lock);
}

static void
external_thumbnailers_disabled_changed_cb (GSettings                    *settings,
                                           const gchar                  *key,
                                           XfceDesktopThumbnailFactory *factory)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;

  g_mutex_lock (&priv->lock);

  if (!priv->disabled)
    {
      g_strfreev (priv->disabled_types);
      priv->disabled_types = g_settings_get_strv (priv->settings, "disable");
    }

  g_mutex_unlock (&priv->lock);
}

static void
xfce_desktop_thumbnail_factory_init (XfceDesktopThumbnailFactory *factory)
{
  XfceDesktopThumbnailFactoryPrivate *priv;
  
  factory->priv = XFCE_DESKTOP_THUMBNAIL_FACTORY_GET_PRIVATE (factory);

  priv = factory->priv;

  priv->size = XFCE_DESKTOP_THUMBNAIL_SIZE_NORMAL;
  
  priv->mime_types_map = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                (GDestroyNotify)g_free,
                                                (GDestroyNotify)thumbnailer_unref);

  g_mutex_init (&priv->lock);

  priv->settings = g_settings_new ("org.mate.thumbnailers");

  g_signal_connect (priv->settings, "changed::disable-all",
                    G_CALLBACK (external_thumbnailers_disabled_all_changed_cb),
                    factory);
  g_signal_connect (priv->settings, "changed::disable",
                    G_CALLBACK (external_thumbnailers_disabled_changed_cb),
                    factory);

  priv->disabled = g_settings_get_boolean (priv->settings, "disable-all");

  if (!priv->disabled)
    priv->disabled_types = g_settings_get_strv (priv->settings, "disable");

  if (!priv->disabled)
    xfce_desktop_thumbnail_factory_load_thumbnailers (factory);
}

static void
xfce_desktop_thumbnail_factory_class_init (XfceDesktopThumbnailFactoryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
	
  gobject_class->finalize = xfce_desktop_thumbnail_factory_finalize;

  g_type_class_add_private (class, sizeof (XfceDesktopThumbnailFactoryPrivate));
}

/**
 * xfce_desktop_thumbnail_factory_new:
 * @size: The thumbnail size to use
 *
 * Creates a new #XfceDesktopThumbnailFactory.
 *
 * This function must be called on the main thread.
 * 
 * Return value: a new #XfceDesktopThumbnailFactory
 *
 * Since: 2.2
 **/
XfceDesktopThumbnailFactory *
xfce_desktop_thumbnail_factory_new (XfceDesktopThumbnailSize size)
{
  XfceDesktopThumbnailFactory *factory;
  
  factory = g_object_new (XFCE_DESKTOP_TYPE_THUMBNAIL_FACTORY, NULL);
  
  factory->priv->size = size;
  
  return factory;
}

/**
 * xfce_desktop_thumbnail_factory_lookup:
 * @factory: a #XfceDesktopThumbnailFactory
 * @uri: the uri of a file
 * @mtime: the mtime of the file
 *
 * Tries to locate an existing thumbnail for the file specified.
 *
 * Usage of this function is threadsafe.
 *
 * Return value: (transfer full): The absolute path of the thumbnail, or %NULL if none exist.
 *
 * Since: 2.2
 **/
char *
xfce_desktop_thumbnail_factory_lookup (XfceDesktopThumbnailFactory *factory,
					const char            *uri,
					time_t                 mtime)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  char *path, *file;
  GChecksum *checksum;
  guint8 digest[16];
  gsize digest_len = sizeof (digest);
  GdkPixbuf *pixbuf;
  gboolean res;

  g_return_val_if_fail (uri != NULL, NULL);

  res = FALSE;

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_assert (digest_len == 16);

  file = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);
  
  path = g_build_filename (g_get_user_cache_dir (),
                           "thumbnails",
                           (priv->size == XFCE_DESKTOP_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
                           file,
                           NULL);
  g_free (file);

  pixbuf = gdk_pixbuf_new_from_file (path, NULL);
  if (pixbuf != NULL)
    {
      res = xfce_desktop_thumbnail_is_valid (pixbuf, uri, mtime);
      g_object_unref (pixbuf);
    }

  g_checksum_free (checksum);

  if (res)
    return path;

  g_free (path);
  return FALSE;
}

/**
 * xfce_desktop_thumbnail_factory_has_valid_failed_thumbnail:
 * @factory: a #XfceDesktopThumbnailFactory
 * @uri: the uri of a file
 * @mtime: the mtime of the file
 *
 * Tries to locate an failed thumbnail for the file specified. Writing
 * and looking for failed thumbnails is important to avoid to try to
 * thumbnail e.g. broken images several times.
 *
 * Usage of this function is threadsafe.
 *
 * Return value: TRUE if there is a failed thumbnail for the file.
 *
 * Since: 2.2
 **/
gboolean
xfce_desktop_thumbnail_factory_has_valid_failed_thumbnail (XfceDesktopThumbnailFactory *factory,
							    const char            *uri,
							    time_t                 mtime)
{
  char *path, *file;
  GdkPixbuf *pixbuf;
  gboolean res;
  GChecksum *checksum;
  guint8 digest[16];
  gsize digest_len = sizeof (digest);

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_assert (digest_len == 16);

  res = FALSE;

  file = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);

  path = g_build_filename (g_get_user_cache_dir (),
                           "thumbnails/fail",
                           appname,
                           file,
                           NULL);
  g_free (file);

  pixbuf = gdk_pixbuf_new_from_file (path, NULL);
  g_free (path);

  if (pixbuf)
    {
      res = xfce_desktop_thumbnail_is_valid (pixbuf, uri, mtime);
      g_object_unref (pixbuf);
    }

  g_checksum_free (checksum);

  return res;
}

/* forbidden/buggy GdkPixbufFormat names */
const char *forbidden[] = { "tga", "icns", "jpeg2000" };

static gboolean
type_is_forbidden (const gchar *name)
{
    gint i = 0;
    for (i = 0; i < G_N_ELEMENTS (forbidden); i++) {
        if (g_strcmp0 (forbidden[i], name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
mimetype_supported_by_gdk_pixbuf (const char *mime_type)
{
    guint i;
    static gsize formats_hash = 0;
    gchar *key;
    gboolean result;

    if (g_once_init_enter (&formats_hash)) {
        GSList *formats, *list;
        GHashTable *hash;

        hash = g_hash_table_new_full (g_str_hash,
                                      (GEqualFunc) g_content_type_equals,
                                      g_free, NULL);

        formats = gdk_pixbuf_get_formats ();
        list = formats;

        while (list) {
            GdkPixbufFormat *format = list->data;
            gchar **mime_types;

            if (type_is_forbidden (format->name)) {
                gdk_pixbuf_format_set_disabled (format, TRUE);
                list = list->next;
                continue;
            }

            mime_types = gdk_pixbuf_format_get_mime_types (format);

            for (i = 0; mime_types[i] != NULL; i++)
                g_hash_table_insert (hash,
                                     (gpointer) g_content_type_from_mime_type (mime_types[i]),
                                     GUINT_TO_POINTER (1));

            g_strfreev (mime_types);
            list = list->next;
        }

        g_slist_free (formats);

        g_once_init_leave (&formats_hash, (gsize) hash);
    }

    key = g_content_type_from_mime_type (mime_type);
    if (g_hash_table_lookup ((void*)formats_hash, key))
            result = TRUE;
    else
            result = FALSE;
    g_free (key);

    return result;
}

/**
 * xfce_desktop_thumbnail_factory_can_thumbnail:
 * @factory: a #XfceDesktopThumbnailFactory
 * @uri: the uri of a file
 * @mime_type: the mime type of the file
 * @mtime: the mtime of the file
 *
 * Returns TRUE if this MateIconFactory can (at least try) to thumbnail
 * this file. Thumbnails or files with failed thumbnails won't be thumbnailed.
 *
 * Usage of this function is threadsafe.
 *
 * Return value: TRUE if the file can be thumbnailed.
 *
 * Since: 2.2
 **/
gboolean
xfce_desktop_thumbnail_factory_can_thumbnail (XfceDesktopThumbnailFactory *factory,
					       const char            *uri,
					       const char            *mime_type,
					       time_t                 mtime)
{
  gboolean have_script = FALSE;

  /* Don't thumbnail thumbnails */
  if (uri &&
      strncmp (uri, "file:/", 6) == 0 &&
      (strstr (uri, "/.thumbnails/") != NULL ||
      strstr (uri, "/.cache/thumbnails/") != NULL))
    return FALSE;
  
  if (!mime_type)
    return FALSE;

  g_mutex_lock (&factory->priv->lock);
  if (!xfce_desktop_thumbnail_factory_is_disabled (factory, mime_type))
    {
      Thumbnailer *thumb;

      thumb = g_hash_table_lookup (factory->priv->mime_types_map, mime_type);
      have_script = thumbnailer_try_exec (thumb);
    }
  g_mutex_unlock (&factory->priv->lock);

  if (have_script || mimetype_supported_by_gdk_pixbuf (mime_type))
    {
      return !xfce_desktop_thumbnail_factory_has_valid_failed_thumbnail (factory,
                                                                          uri,
                                                                          mtime);
    }
  
  return FALSE;
}

static gboolean
make_thumbnail_dirs (XfceDesktopThumbnailFactory *factory)
{
  char *thumbnail_dir;
  char *image_dir;
  gboolean res;

  res = FALSE;

  thumbnail_dir = g_build_filename (g_get_user_cache_dir (),
                                    "thumbnails",
                                    NULL);
  if (!g_file_test (thumbnail_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir (thumbnail_dir, 0700);
      res = TRUE;
    }

  image_dir = g_build_filename (thumbnail_dir,
				(factory->priv->size == XFCE_DESKTOP_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
				NULL);
  if (!g_file_test (image_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir (image_dir, 0700);
      res = TRUE;
    }

  g_free (thumbnail_dir);
  g_free (image_dir);
  
  return res;
}

static gboolean
make_thumbnail_fail_dirs (XfceDesktopThumbnailFactory *factory)
{
  char *thumbnail_dir;
  char *fail_dir;
  char *app_dir;
  gboolean res;

  res = FALSE;

  thumbnail_dir = g_build_filename (g_get_user_cache_dir (),
                                    "thumbnails",
                                    NULL);
  if (!g_file_test (thumbnail_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir (thumbnail_dir, 0700);
      res = TRUE;
    }

  fail_dir = g_build_filename (thumbnail_dir,
			       "fail",
			       NULL);
  if (!g_file_test (fail_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir (fail_dir, 0700);
      res = TRUE;
    }

  app_dir = g_build_filename (fail_dir,
			      appname,
			      NULL);
  if (!g_file_test (app_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir (app_dir, 0700);
      res = TRUE;
    }

  g_free (thumbnail_dir);
  g_free (fail_dir);
  g_free (app_dir);
  
  return res;
}


/**
 * xfce_desktop_thumbnail_factory_save_thumbnail:
 * @factory: a #XfceDesktopThumbnailFactory
 * @thumbnail: the thumbnail as a pixbuf 
 * @uri: the uri of a file
 * @original_mtime: the modification time of the original file 
 *
 * Saves @thumbnail at the right place. If the save fails a
 * failed thumbnail is written.
 *
 * Usage of this function is threadsafe.
 *
 * Since: 2.2
 **/
void
xfce_desktop_thumbnail_factory_save_thumbnail (XfceDesktopThumbnailFactory *factory,
						GdkPixbuf             *thumbnail,
						const char            *uri,
						time_t                 original_mtime)
{
  XfceDesktopThumbnailFactoryPrivate *priv = factory->priv;
  char *path, *file;
  char *tmp_path;
  const char *width, *height;
  int tmp_fd;
  char mtime_str[21];
  gboolean saved_ok;
  GChecksum *checksum;
  guint8 digest[16];
  gsize digest_len = sizeof (digest);
  GError *error;

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_assert (digest_len == 16);

  file = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);

  path = g_build_filename (g_get_user_cache_dir (),
                           "thumbnails",
                           (priv->size == XFCE_DESKTOP_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
                           file,
                           NULL);

  g_free (file);

  g_checksum_free (checksum);

  tmp_path = g_strconcat (path, ".XXXXXX", NULL);

  tmp_fd = g_mkstemp (tmp_path);
  if (tmp_fd == -1 &&
      make_thumbnail_dirs (factory))
    {
      g_free (tmp_path);
      tmp_path = g_strconcat (path, ".XXXXXX", NULL);
      tmp_fd = g_mkstemp (tmp_path);
    }

  if (tmp_fd == -1)
    {
      xfce_desktop_thumbnail_factory_create_failed_thumbnail (factory, uri, original_mtime);
      g_free (tmp_path);
      g_free (path);
      return;
    }
  close (tmp_fd);
  
  g_snprintf (mtime_str, 21, "%ld",  original_mtime);
  width = gdk_pixbuf_get_option (thumbnail, "tEXt::Thumb::Image::Width");
  height = gdk_pixbuf_get_option (thumbnail, "tEXt::Thumb::Image::Height");

  error = NULL;
  if (width != NULL && height != NULL) 
    saved_ok  = gdk_pixbuf_save (thumbnail,
				 tmp_path,
				 "png", &error,
				 "tEXt::Thumb::Image::Width", width,
				 "tEXt::Thumb::Image::Height", height,
				 "tEXt::Thumb::URI", uri,
				 "tEXt::Thumb::MTime", mtime_str,
				 "tEXt::Software", "MATE::ThumbnailFactory",
				 NULL);
  else
    saved_ok  = gdk_pixbuf_save (thumbnail,
				 tmp_path,
				 "png", &error,
				 "tEXt::Thumb::URI", uri,
				 "tEXt::Thumb::MTime", mtime_str,
				 "tEXt::Software", "MATE::ThumbnailFactory",
				 NULL);
    

  if (saved_ok)
    {
      g_chmod (tmp_path, 0600);
      g_rename (tmp_path, path);
    }
  else
    {
      g_warning ("Failed to create thumbnail %s: %s", tmp_path, error->message);
      xfce_desktop_thumbnail_factory_create_failed_thumbnail (factory, uri, original_mtime);
      g_unlink (tmp_path);
      g_clear_error (&error);
    }

  g_free (path);
  g_free (tmp_path);
}

/**
 * xfce_desktop_thumbnail_factory_create_failed_thumbnail:
 * @factory: a #XfceDesktopThumbnailFactory
 * @uri: the uri of a file
 * @mtime: the modification time of the file
 *
 * Creates a failed thumbnail for the file so that we don't try
 * to re-thumbnail the file later.
 *
 * Usage of this function is threadsafe.
 *
 * Since: 2.2
 **/
void
xfce_desktop_thumbnail_factory_create_failed_thumbnail (XfceDesktopThumbnailFactory *factory,
							 const char            *uri,
							 time_t                 mtime)
{
  char *path, *file;
  char *tmp_path;
  int tmp_fd;
  char mtime_str[21];
  gboolean saved_ok;
  GdkPixbuf *pixbuf;
  GChecksum *checksum;
  guint8 digest[16];
  gsize digest_len = sizeof (digest);

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_assert (digest_len == 16);

  file = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);

  path = g_build_filename (g_get_user_cache_dir (),
                           "thumbnails/fail",
                           appname,
                           file,
                           NULL);
  g_free (file);

  g_checksum_free (checksum);

  tmp_path = g_strconcat (path, ".XXXXXX", NULL);

  tmp_fd = g_mkstemp (tmp_path);
  if (tmp_fd == -1 &&
      make_thumbnail_fail_dirs (factory))
    {
      g_free (tmp_path);
      tmp_path = g_strconcat (path, ".XXXXXX", NULL);
      tmp_fd = g_mkstemp (tmp_path);
    }

  if (tmp_fd == -1)
    {
      g_free (tmp_path);
      g_free (path);
      return;
    }
  close (tmp_fd);
  
  g_snprintf (mtime_str, 21, "%ld",  mtime);
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  saved_ok  = gdk_pixbuf_save (pixbuf,
			       tmp_path,
			       "png", NULL, 
			       "tEXt::Thumb::URI", uri,
			       "tEXt::Thumb::MTime", mtime_str,
			       "tEXt::Software", "MATE::ThumbnailFactory",
			       NULL);
  g_object_unref (pixbuf);
  if (saved_ok)
    {
      g_chmod (tmp_path, 0600);
      g_rename(tmp_path, path);
    }

  g_free (path);
  g_free (tmp_path);
}

/**
 * xfce_desktop_thumbnail_md5:
 * @uri: an uri
 *
 * Calculates the MD5 checksum of the uri. This can be useful
 * if you want to manually handle thumbnail files.
 *
 * Return value: A string with the MD5 digest of the uri string.
 *
 * Since: 2.2
 * Deprecated: 2.22: Use #GChecksum instead
 **/
char *
xfce_desktop_thumbnail_md5 (const char *uri)
{
  return g_compute_checksum_for_data (G_CHECKSUM_MD5,
                                      (const guchar *) uri,
                                      strlen (uri));
}

/**
 * xfce_desktop_thumbnail_path_for_uri:
 * @uri: an uri
 * @size: a thumbnail size
 *
 * Returns the filename that a thumbnail of size @size for @uri would have.
 *
 * Return value: an absolute filename
 *
 * Since: 2.2
 **/
char *
xfce_desktop_thumbnail_path_for_uri (const char         *uri,
				      XfceDesktopThumbnailSize  size)
{
  char *md5;
  char *file;
  char *path;

  md5 = xfce_desktop_thumbnail_md5 (uri);
  file = g_strconcat (md5, ".png", NULL);
  g_free (md5);
  
  path = g_build_filename (g_get_user_cache_dir (),
                           "thumbnails",
                           (size == XFCE_DESKTOP_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
                           file,
                           NULL);
  g_free (file);

  return path;
}

/**
 * xfce_desktop_thumbnail_has_uri:
 * @pixbuf: an loaded thumbnail pixbuf
 * @uri: a uri
 *
 * Returns whether the thumbnail has the correct uri embedded in the
 * Thumb::URI option in the png.
 *
 * Return value: TRUE if the thumbnail is for @uri
 *
 * Since: 2.2
 **/
gboolean
xfce_desktop_thumbnail_has_uri (GdkPixbuf          *pixbuf,
				 const char         *uri)
{
  const char *thumb_uri;
  
  thumb_uri = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::URI");
  if (!thumb_uri)
    return FALSE;

  return strcmp (uri, thumb_uri) == 0;
}

/**
 * xfce_desktop_thumbnail_is_valid:
 * @pixbuf: an loaded thumbnail #GdkPixbuf
 * @uri: a uri
 * @mtime: the mtime
 *
 * Returns whether the thumbnail has the correct uri and mtime embedded in the
 * png options.
 *
 * Return value: TRUE if the thumbnail has the right @uri and @mtime
 *
 * Since: 2.2
 **/
gboolean
xfce_desktop_thumbnail_is_valid (GdkPixbuf          *pixbuf,
				  const char         *uri,
				  time_t              mtime)
{
  const char *thumb_uri, *thumb_mtime_str;
  time_t thumb_mtime;
  
  thumb_uri = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::URI");
  if (!thumb_uri)
    return FALSE;
  if (strcmp (uri, thumb_uri) != 0)
    return FALSE;
  
  thumb_mtime_str = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::MTime");
  if (!thumb_mtime_str)
    return FALSE;
  thumb_mtime = atol (thumb_mtime_str);
  if (mtime != thumb_mtime)
    return FALSE;
  
  return TRUE;
}
