/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 John Audia <therealgraysky@proton.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * Read the hack selection from an xscreensaver config (~/.xscreensaver,
 * merged with the XScreenSaver app-defaults file) so the user's own list
 * of enabled hacks, with their arguments, can be used directly.
 *
 * Only "programs", "mode" and "selected" are read. The parsing follows
 * xscreensaver's own reader: a trailing backslash continues a line,
 * backslash escapes are decoded (so "\n" separates programs), and each
 * program has the form:
 *
 *     [-] [visual:] ["Label"] command [args...]
 */

#include <string.h>
#include <sys/stat.h>

#include <glib/gstdio.h>

#include "gs-debug.h"
#include "gs-xss-config.h"

/* ~/.xscreensaver is read on every activation; refuse anything odd */
#define MAX_CONFIG_SIZE (1024 * 1024)

static const gchar *app_defaults_files[] = {
    "/usr/share/X11/app-defaults/XScreenSaver",
    "/etc/X11/app-defaults/XScreenSaver",
    NULL
};

static void
program_free (gpointer data) {
    GSXssProgram *program = data;

    g_free (program->command);
    g_free (program);
}

static GSXssProgram *
program_new (const gchar *command,
             gboolean enabled) {
    GSXssProgram *program = g_new0 (GSXssProgram, 1);

    program->command = g_strdup (command);
    program->enabled = enabled;

    return program;
}

static GSXssConfig *
config_new (void) {
    GSXssConfig *config = g_new0 (GSXssConfig, 1);

    config->selected = -1;
    config->programs = g_ptr_array_new_with_free_func (program_free);

    return config;
}

void
gs_xss_config_free (GSXssConfig *config) {
    if (config == NULL) {
        return;
    }

    g_free (config->mode);
    g_ptr_array_unref (config->programs);
    g_free (config);
}

static const gchar *
skip_space (const gchar *p) {
    while (*p != '\0' && g_ascii_isspace (*p)) {
        p++;
    }

    return p;
}

/* Matches xscreensaver's parse_screenhack(). An entry with no command
 * keeps its slot (command NULL) so "selected" indexes stay aligned. */
static GSXssProgram *
parse_program (const gchar *entry) {
    const gchar *p = skip_space (entry);
    const gchar *q;
    gboolean enabled = TRUE;
    GSXssProgram *program;
    gchar *command;

    if (*p == '-') {
        enabled = FALSE;
        p = skip_space (p + 1);
    }

    /* optional visual, e.g. "GL:" or "default:" */
    for (q = p; *q != '\0' && *q != ':' && *q != '"' && !g_ascii_isspace (*q); q++) {
    }
    if (*q == ':') {
        p = skip_space (q + 1);
    }

    /* optional "Label" */
    if (*p == '"') {
        q = strchr (p + 1, '"');
        p = (q != NULL) ? skip_space (q + 1) : p + strlen (p);
    }

    command = g_strstrip (g_strdup (p));
    program = program_new (*command != '\0' ? command : NULL, enabled);
    g_free (command);

    return program;
}

static void
parse_programs (GSXssConfig *config,
                const gchar *value) {
    gchar **entries;

    g_ptr_array_set_size (config->programs, 0);

    entries = g_strsplit (value, "\n", -1);
    for (guint i = 0; entries[i] != NULL; i++) {
        if (*skip_space (entries[i]) != '\0') {
            g_ptr_array_add (config->programs, parse_program (entries[i]));
        }
    }
    g_strfreev (entries);
}

/* Map "programs", "*programs", "XScreenSaver.programs" and
 * "XScreenSaver*programs" to "programs". Anything more specific,
 * such as per-hack resources in app-defaults, returns NULL. */
static const gchar *
resource_name (const gchar *key) {
    const gchar *name = key;
    gsize prefix = strlen ("XScreenSaver");

    if (g_ascii_strncasecmp (name, "XScreenSaver", prefix) == 0) {
        name += prefix;
        if (*name != '*' && *name != '.') {
            return NULL;
        }
    }
    if (*name == '*' || *name == '.') {
        name++;
    }
    if (*name == '\0' || strpbrk (name, "*.") != NULL) {
        return NULL;
    }

    return name;
}

static gboolean
name_is (const gchar *name,
         const gchar *wanted) {
    return name != NULL && g_ascii_strcasecmp (name, wanted) == 0;
}

/* Decode backslash escapes in place, like xscreensaver's init file reader */
static void
decode_escapes (gchar *line) {
    gchar *in = line;
    gchar *out = line;

    while (*in != '\0') {
        if (*in != '\\') {
            *out++ = *in++;
            continue;
        }

        in++;
        switch (*in) {
            case 'n':
                *out++ = '\n';
                break;
            case 'r':
                *out++ = '\r';
                break;
            case 't':
                *out++ = '\t';
                break;
            case '\0':
                *out = '\0';
                return;
            default:
                *out++ = *in;
                break;
        }
        in++;
    }

    *out = '\0';
}

static void
parse_line (GSXssConfig *config,
            gchar *line) {
    const gchar *p;
    const gchar *colon;
    const gchar *name;
    const gchar *value;
    gchar *key;

    decode_escapes (line);

    p = skip_space (line);
    if (*p == '\0' || *p == '#' || *p == '!' || *p == ';') {
        return;
    }

    colon = strchr (p, ':');
    if (colon == NULL) {
        return;
    }

    key = g_strstrip (g_strndup (p, colon - p));
    value = skip_space (colon + 1);
    name = resource_name (key);

    if (name_is (name, "programs")) {
        parse_programs (config, value);
    } else if (name_is (name, "mode")) {
        g_free (config->mode);
        config->mode = g_strstrip (g_strdup (value));
    } else if (name_is (name, "selected")) {
        gchar *end;
        gint64 selected = g_ascii_strtoll (value, &end, 10);

        if (end != value) {
            config->selected = (selected >= 0 && selected <= G_MAXINT) ? (gint) selected : -1;
        }
    }

    g_free (key);
}

GSXssConfig *
gs_xss_config_parse (const gchar *contents) {
    GSXssConfig *config;
    GString *logical;
    gchar **lines;

    g_return_val_if_fail (contents != NULL, NULL);

    config = config_new ();
    logical = g_string_new (NULL);
    lines = g_strsplit (contents, "\n", -1);

    for (guint i = 0; lines[i] != NULL; i++) {
        gchar *line = lines[i];
        gsize len = strlen (line);

        if (len > 0 && line[len - 1] == '\r') {
            line[--len] = '\0';
        }

        /* trailing backslash continues the line */
        if (len > 0 && line[len - 1] == '\\') {
            g_string_append_len (logical, line, len - 1);
            continue;
        }

        g_string_append (logical, line);
        parse_line (config, logical->str);
        g_string_truncate (logical, 0);
    }

    if (logical->len > 0) {
        parse_line (config, logical->str);
    }

    g_strfreev (lines);
    g_string_free (logical, TRUE);

    return config;
}

static gboolean
is_assignment (const gchar *word) {
    if (!g_ascii_isalpha (*word) && *word != '_') {
        return FALSE;
    }
    for (word++; g_ascii_isalnum (*word) || *word == '_'; word++) {
    }

    return *word == '=';
}

/* Skip leading VAR=value words, the way the shell does */
static const gchar *
skip_assignments (const gchar *command) {
    const gchar *p = skip_space (command);

    while (is_assignment (p)) {
        while (*p != '\0' && !g_ascii_isspace (*p)) {
            p++;
        }
        p = skip_space (p);
    }

    return p;
}

/* The program name, used to match user entries against system ones */
static gchar *
program_name (const gchar *command) {
    const gchar *start;
    const gchar *end;
    gchar *word;
    gchar *name;

    if (command == NULL) {
        return NULL;
    }

    start = skip_assignments (command);
    for (end = start; *end != '\0' && !g_ascii_isspace (*end); end++) {
    }
    if (end == start) {
        return NULL;
    }

    word = g_strndup (start, end - start);
    name = g_path_get_basename (word);
    g_free (word);

    return name;
}

/* Append app-defaults programs missing from the user's list, keeping their
 * enabled state, as xscreensaver's merge_system_screenhacks() does. */
void
gs_xss_config_merge (GSXssConfig *config,
                     const GSXssConfig *defaults) {
    GHashTable *names;

    g_return_if_fail (config != NULL);

    if (defaults == NULL) {
        return;
    }

    names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    for (guint i = 0; i < config->programs->len; i++) {
        GSXssProgram *program = g_ptr_array_index (config->programs, i);
        gchar *name = program_name (program->command);

        if (name != NULL) {
            g_hash_table_add (names, g_ascii_strdown (name, -1));
            g_free (name);
        }
    }

    for (guint i = 0; i < defaults->programs->len; i++) {
        GSXssProgram *program = g_ptr_array_index (defaults->programs, i);
        gchar *name = program_name (program->command);
        gchar *key;

        if (name == NULL) {
            continue;
        }

        key = g_ascii_strdown (name, -1);
        g_free (name);

        if (g_hash_table_contains (names, key)) {
            g_free (key);
            continue;
        }

        g_ptr_array_add (config->programs, program_new (program->command, program->enabled));
        g_hash_table_add (names, key);
    }

    g_hash_table_destroy (names);
}

static GSXssConfig *
load_file (const gchar *path) {
    GSXssConfig *config;
    GError *error = NULL;
    GStatBuf st;
    gchar *contents;

    if (g_stat (path, &st) != 0) {
        return NULL;
    }
    if (!S_ISREG (st.st_mode) || st.st_size > MAX_CONFIG_SIZE) {
        gs_debug ("Ignoring %s: not a regular file or too large", path);
        return NULL;
    }

    if (!g_file_get_contents (path, &contents, NULL, &error)) {
        gs_debug ("Could not read %s: %s", path, error->message);
        g_error_free (error);
        return NULL;
    }

    config = gs_xss_config_parse (contents);
    g_free (contents);

    gs_debug ("Read %u xscreensaver programs from %s", config->programs->len, path);

    return config;
}

/* Load ~/.xscreensaver merged with the system app-defaults programs.
 * Returns NULL if neither exists. */
GSXssConfig *
gs_xss_config_load (void) {
    GSXssConfig *config;
    GSXssConfig *defaults = NULL;
    gchar *path;

    path = g_build_filename (g_get_home_dir (), ".xscreensaver", NULL);
    config = load_file (path);
    g_free (path);

    for (guint i = 0; app_defaults_files[i] != NULL && defaults == NULL; i++) {
        defaults = load_file (app_defaults_files[i]);
        if (defaults != NULL && defaults->programs->len == 0) {
            g_clear_pointer (&defaults, gs_xss_config_free);
        }
    }

    if (config == NULL) {
        return defaults;
    }

    gs_xss_config_merge (config, defaults);
    gs_xss_config_free (defaults);

    return config;
}

/* TRUE if the program the command runs can be found in PATH. The theme
 * manager adds the xscreensaver hack directories to PATH on creation. */
gboolean
gs_xss_config_command_available (const gchar *command,
                                 gpointer user_data) {
    gchar **argv = NULL;
    gchar *path = NULL;
    guint i = 0;

    if (command == NULL || !g_shell_parse_argv (command, NULL, &argv, NULL)) {
        return FALSE;
    }

    while (argv[i] != NULL && is_assignment (argv[i])) {
        i++;
    }
    if (argv[i] != NULL) {
        path = g_find_program_in_path (argv[i]);
    }

    g_strfreev (argv);

    if (path == NULL) {
        return FALSE;
    }

    g_free (path);
    return TRUE;
}

static gboolean
program_usable (const GSXssProgram *program,
                GSXssAvailableFunc available,
                gpointer user_data) {
    return program->command != NULL
           && (available == NULL || available (program->command, user_data));
}

/* Returns the command to run, or NULL for a blank screen. "random-same"
 * is treated as "random"; each monitor gets its own pick. "previous" is
 * avoided when there is anything else to choose. */
const gchar *
gs_xss_config_pick (const GSXssConfig *config,
                    const gchar *previous,
                    GSXssAvailableFunc available,
                    gpointer user_data) {
    GPtrArray *candidates;
    GPtrArray *fresh;
    GPtrArray *pool;
    const gchar *command = NULL;

    if (config == NULL) {
        return NULL;
    }

    if (name_is (config->mode, "blank") || name_is (config->mode, "off")) {
        return NULL;
    }

    if (name_is (config->mode, "one")
        && config->selected >= 0
        && (guint) config->selected < config->programs->len) {
        GSXssProgram *program = g_ptr_array_index (config->programs, config->selected);

        if (program_usable (program, available, user_data)) {
            return program->command;
        }
    }

    candidates = g_ptr_array_new ();
    fresh = g_ptr_array_new ();
    for (guint i = 0; i < config->programs->len; i++) {
        GSXssProgram *program = g_ptr_array_index (config->programs, i);

        if (program->enabled && program_usable (program, available, user_data)) {
            g_ptr_array_add (candidates, program->command);
            if (g_strcmp0 (program->command, previous) != 0) {
                g_ptr_array_add (fresh, program->command);
            }
        }
    }

    pool = (fresh->len > 0) ? fresh : candidates;
    if (pool->len > 0) {
        command = g_ptr_array_index (pool, g_random_int_range (0, pool->len));
    }

    g_ptr_array_unref (fresh);
    g_ptr_array_unref (candidates);

    return command;
}

/* xscreensaver runs programs through the shell, with "exec" placed after
 * any leading VAR=value words so the hack replaces the shell and the
 * job's pid is the hack itself. */
gchar *
gs_xss_config_wrap_command (const gchar *command) {
    const gchar *start;
    const gchar *rest;
    gchar *script;
    gchar *quoted;
    gchar *wrapped;

    g_return_val_if_fail (command != NULL, NULL);

    start = skip_space (command);
    rest = skip_assignments (start);

    script = g_strdup_printf ("%.*sexec %s", (int) (rest - start), start, rest);
    quoted = g_shell_quote (script);
    wrapped = g_strconcat ("/bin/sh -c ", quoted, NULL);

    g_free (script);
    g_free (quoted);

    return wrapped;
}

/* Load the user's xscreensaver config and return a command ready for
 * GSJob, or NULL for a blank screen. */
gchar *
gs_xss_config_get_command (void) {
    static gchar *last_command = NULL;
    GSXssConfig *config;
    const gchar *command;
    gchar *wrapped = NULL;

    config = gs_xss_config_load ();
    if (config == NULL) {
        gs_debug ("No xscreensaver configuration found");
        return NULL;
    }

    command = gs_xss_config_pick (config, last_command, gs_xss_config_command_available, NULL);
    if (command != NULL) {
        gs_debug ("Picked xscreensaver program: %s", command);
        g_free (last_command);
        last_command = g_strdup (command);
        wrapped = gs_xss_config_wrap_command (command);
    } else {
        gs_debug ("No usable program in xscreensaver configuration (mode: %s)",
                  config->mode != NULL ? config->mode : "unset");
    }

    gs_xss_config_free (config);

    return wrapped;
}
