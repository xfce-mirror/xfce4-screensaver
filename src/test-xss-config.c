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

/* release builds define G_DISABLE_ASSERT, which turns g_test_init() into exit(1) */
#undef G_DISABLE_ASSERT

#include "gs-xss-config.h"

/* Excerpt in the layout xscreensaver-settings 6.x writes */
static const gchar *user_config =
    "# XScreenSaver Preferences File\n"
    "timeout:\t0:10:00\n"
    "mode:\t\trandom\n"
    "selected:\t3\n"
    "textProgram:\tfortune\n"
    "\n"
    "programs:\t\t\t\t\t\t\t\t      \\\n"
    "-\t\t     \"ATV4-2k\" \tatv4-2k\t\t\t\t\t    \\n\\\n"
    "-\t\t\t\tmaze --root\t\t\t\t    \\n\\\n"
    "- GL: \t\t\t\tsuperquadrics --root\t\t\t    \\n\\\n"
    "\t\t\t\tpyro --root --delay 34146 --count 646\t      \\\n"
    "\t\t\t\t  --frequency 86 --scatter 286\t\t    \\n\\\n"
    "\t\t\t\tgrav --root\t\t\t\t    \\n\\\n"
    "- GL: \t\t\t\tmolecule -root -no-labels -no-atoms\t    \\n\\\n"
    "\t\t\t\tphosphor --root --delay 24590 --scale 4\t      \\\n"
    "\t\t\t\t  --ticks 17 --fg white\t\t\t    \\n\\\n"
    "\n"
    "\n"
    "pointerHysteresis:  10\n";

/* Compare commands by their shell words, so continuation whitespace
 * does not matter */
static void
assert_command (const gchar *command,
                const gchar *expected) {
    gchar **argv = NULL;
    gchar **expected_argv = NULL;

    g_assert_nonnull (command);
    g_assert_true (g_shell_parse_argv (command, NULL, &argv, NULL));
    g_assert_true (g_shell_parse_argv (expected, NULL, &expected_argv, NULL));
    g_assert_cmpuint (g_strv_length (argv), ==, g_strv_length (expected_argv));
    for (guint i = 0; argv[i] != NULL; i++) {
        g_assert_cmpstr (argv[i], ==, expected_argv[i]);
    }

    g_strfreev (argv);
    g_strfreev (expected_argv);
}

static GSXssProgram *
program_at (GSXssConfig *config,
            guint idx) {
    g_assert_cmpuint (idx, <, config->programs->len);
    return g_ptr_array_index (config->programs, idx);
}

static void
test_parse_user_config (void) {
    GSXssConfig *config = gs_xss_config_parse (user_config);

    g_assert_cmpstr (config->mode, ==, "random");
    g_assert_cmpint (config->selected, ==, 3);
    g_assert_cmpuint (config->programs->len, ==, 7);

    assert_command (program_at (config, 0)->command, "atv4-2k");
    g_assert_false (program_at (config, 0)->enabled);
    assert_command (program_at (config, 1)->command, "maze --root");
    g_assert_false (program_at (config, 1)->enabled);
    assert_command (program_at (config, 2)->command, "superquadrics --root");
    g_assert_false (program_at (config, 2)->enabled);
    assert_command (program_at (config, 3)->command,
                    "pyro --root --delay 34146 --count 646 --frequency 86 --scatter 286");
    g_assert_true (program_at (config, 3)->enabled);
    assert_command (program_at (config, 4)->command, "grav --root");
    g_assert_true (program_at (config, 4)->enabled);
    assert_command (program_at (config, 5)->command, "molecule -root -no-labels -no-atoms");
    g_assert_false (program_at (config, 5)->enabled);
    assert_command (program_at (config, 6)->command,
                    "phosphor --root --delay 24590 --scale 4 --ticks 17 --fg white");
    g_assert_true (program_at (config, 6)->enabled);

    gs_xss_config_free (config);
}

static void
test_parse_app_defaults (void) {
    GSXssConfig *config = gs_xss_config_parse (
        "! comment\n"
        "; also a comment\n"
        "*Mode:\t\tOne\n"
        "XScreenSaver.selected:\t1\n"
        "*hacks.glmatrix.name: GLMatrix\n"
        "xscreensaver*Programs:\t\\\n"
        "  default: \"Label\" qix -root \\n\\\n"
        "  GL: glmatrix -root \\n\\\n"
        "- GL: \"My Name\" glhack --root \\n\n");

    g_assert_cmpstr (config->mode, ==, "One");
    g_assert_cmpint (config->selected, ==, 1);
    g_assert_cmpuint (config->programs->len, ==, 3);
    assert_command (program_at (config, 0)->command, "qix -root");
    assert_command (program_at (config, 1)->command, "glmatrix -root");
    g_assert_true (program_at (config, 1)->enabled);
    assert_command (program_at (config, 2)->command, "glhack --root");
    g_assert_false (program_at (config, 2)->enabled);

    gs_xss_config_free (config);
}

static void
test_parse_edge_cases (void) {
    GSXssConfig *config;

    /* CRLF; an entry with no command keeps its slot; no trailing newline */
    config = gs_xss_config_parse ("mode: blank\r\n"
                                  "selected: nope\r\n"
                                  "programs: \"Broken qix \\n \\n  \\n hop --root");
    g_assert_cmpstr (config->mode, ==, "blank");
    g_assert_cmpint (config->selected, ==, -1);
    g_assert_cmpuint (config->programs->len, ==, 2);
    g_assert_null (program_at (config, 0)->command);
    assert_command (program_at (config, 1)->command, "hop --root");
    gs_xss_config_free (config);

    /* a later programs resource replaces an earlier one */
    config = gs_xss_config_parse ("programs: a\\n b\n"
                                  "programs: c\n"
                                  "selected: -4\n");
    g_assert_cmpuint (config->programs->len, ==, 1);
    assert_command (program_at (config, 0)->command, "c");
    g_assert_cmpint (config->selected, ==, -1);
    gs_xss_config_free (config);

    /* escapes: "\\n" is a backslash and an n, not a separator */
    config = gs_xss_config_parse ("programs: foo a\\\\nb \\\"q\\\" \\n bar\n");
    g_assert_cmpuint (config->programs->len, ==, 2);
    g_assert_cmpstr (program_at (config, 0)->command, ==, "foo a\\nb \"q\"");
    g_assert_cmpstr (program_at (config, 1)->command, ==, "bar");
    gs_xss_config_free (config);

    config = gs_xss_config_parse ("");
    g_assert_null (config->mode);
    g_assert_cmpuint (config->programs->len, ==, 0);
    gs_xss_config_free (config);
}

static void
test_merge (void) {
    GSXssConfig *config = gs_xss_config_parse ("programs: a --x \\n - b \\n \"Empty\"\n");
    GSXssConfig *defaults = gs_xss_config_parse (
        "*programs: FOO=1 /usr/lib/xscreensaver/A \\n c --root \\n - d --root \\n b \\n \"Empty\"\n");

    gs_xss_config_merge (config, defaults);

    /* user entries untouched, missing ones appended with their state */
    g_assert_cmpuint (config->programs->len, ==, 5);
    assert_command (program_at (config, 0)->command, "a --x");
    g_assert_false (program_at (config, 1)->enabled);
    g_assert_null (program_at (config, 2)->command);
    assert_command (program_at (config, 3)->command, "c --root");
    g_assert_true (program_at (config, 3)->enabled);
    assert_command (program_at (config, 4)->command, "d --root");
    g_assert_false (program_at (config, 4)->enabled);

    gs_xss_config_merge (config, NULL);
    g_assert_cmpuint (config->programs->len, ==, 5);

    gs_xss_config_free (defaults);
    gs_xss_config_free (config);
}

static gboolean
available_except (const gchar *command,
                  gpointer user_data) {
    return !g_str_has_prefix (command, user_data);
}

static void
test_pick (void) {
    GSXssConfig *config = gs_xss_config_parse (user_config);
    GSXssConfig *pair;

    /* random only ever returns enabled entries */
    for (guint i = 0; i < 200; i++) {
        const gchar *command = gs_xss_config_pick (config, NULL, NULL, NULL);

        g_assert_true (command == program_at (config, 3)->command
                       || command == program_at (config, 4)->command
                       || command == program_at (config, 6)->command);
    }

    /* unavailable entries are skipped */
    for (guint i = 0; i < 50; i++) {
        assert_command (gs_xss_config_pick (config, NULL, available_except, "p"), "grav --root");
    }

    /* nothing usable means blank */
    g_assert_null (gs_xss_config_pick (config, NULL, available_except, ""));

    /* "one" (any case) runs the selected entry, even if it is disabled */
    g_free (config->mode);
    config->mode = g_strdup ("One");
    config->selected = 2;
    assert_command (gs_xss_config_pick (config, NULL, NULL, NULL), "superquadrics --root");

    /* ... and falls back to random if it can't run */
    for (guint i = 0; i < 50; i++) {
        const gchar *command = gs_xss_config_pick (config, NULL, available_except, "su");

        g_assert_true (command == program_at (config, 3)->command
                       || command == program_at (config, 4)->command
                       || command == program_at (config, 6)->command);
    }
    config->selected = 99;
    g_assert_nonnull (gs_xss_config_pick (config, NULL, NULL, NULL));

    g_free (config->mode);
    config->mode = g_strdup ("off");
    g_assert_null (gs_xss_config_pick (config, NULL, NULL, NULL));
    g_free (config->mode);
    config->mode = g_strdup ("Blank");
    g_assert_null (gs_xss_config_pick (config, NULL, NULL, NULL));

    g_assert_null (gs_xss_config_pick (NULL, NULL, NULL, NULL));

    gs_xss_config_free (config);

    /* the previous pick is avoided when there is a choice */
    pair = gs_xss_config_parse ("programs: x \\n y \\n - z\n");
    for (guint i = 0; i < 50; i++) {
        g_assert_cmpstr (gs_xss_config_pick (pair, "x", NULL, NULL), ==, "y");
    }
    g_assert_cmpstr (gs_xss_config_pick (pair, "y", available_except, "x"), ==, "y");
    gs_xss_config_free (pair);
}

static void
assert_wrapped (const gchar *command,
                const gchar *script) {
    gchar *wrapped = gs_xss_config_wrap_command (command);
    gchar **argv = NULL;

    g_assert_true (g_shell_parse_argv (wrapped, NULL, &argv, NULL));
    g_assert_cmpuint (g_strv_length (argv), ==, 3);
    g_assert_cmpstr (argv[0], ==, "/bin/sh");
    g_assert_cmpstr (argv[1], ==, "-c");
    g_assert_cmpstr (argv[2], ==, script);

    g_strfreev (argv);
    g_free (wrapped);
}

static void
test_wrap_command (void) {
    assert_wrapped ("phosphor --root --program 'echo it'\"'\"'s'",
                    "exec phosphor --root --program 'echo it'\"'\"'s'");
    assert_wrapped ("  FOO=1 BAR=x  glhack --root",
                    "FOO=1 BAR=x  exec glhack --root");
    assert_wrapped ("=odd cmd", "exec =odd cmd");
}

static void
test_command_available (void) {
    g_assert_true (gs_xss_config_command_available ("sh -c true", NULL));
    g_assert_true (gs_xss_config_command_available ("FOO=1 BAR=2 sh -c true", NULL));
    g_assert_false (gs_xss_config_command_available ("no-such-hack-xfce4-screensaver --root", NULL));
    g_assert_false (gs_xss_config_command_available ("FOO=1", NULL));
    g_assert_false (gs_xss_config_command_available ("unbalanced 'quote", NULL));
    g_assert_false (gs_xss_config_command_available (NULL, NULL));
}

int
main (int argc,
      char **argv) {
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/xss-config/parse/user-config", test_parse_user_config);
    g_test_add_func ("/xss-config/parse/app-defaults", test_parse_app_defaults);
    g_test_add_func ("/xss-config/parse/edge-cases", test_parse_edge_cases);
    g_test_add_func ("/xss-config/merge", test_merge);
    g_test_add_func ("/xss-config/pick", test_pick);
    g_test_add_func ("/xss-config/wrap-command", test_wrap_command);
    g_test_add_func ("/xss-config/command-available", test_command_available);

    return g_test_run ();
}
