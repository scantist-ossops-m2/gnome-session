/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * save-session.c - Small program to talk to session manager.

   Copyright (C) 1998 Tom Tromey
   Copyright (C) 2008 Red Hat, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.
*/

#include <config.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define GSM_SERVICE_DBUS   "org.gnome.SessionManager"
#define GSM_PATH_DBUS      "/org/gnome/SessionManager"
#define GSM_INTERFACE_DBUS "org.gnome.SessionManager"

enum {
        GSM_LOGOUT_MODE_NORMAL = 0,
        GSM_LOGOUT_MODE_NO_CONFIRMATION,
        GSM_LOGOUT_MODE_FORCE
};

static gboolean logout = FALSE;
static gboolean power_off = FALSE;
static gboolean no_prompt = FALSE;
static gboolean force = FALSE;

static GOptionEntry options[] = {
        {"logout", '\0', 0, G_OPTION_ARG_NONE, &logout, N_("Log out"), NULL},
        {"power-off", '\0', 0, G_OPTION_ARG_NONE, &power_off, N_("Power off"), NULL},
        {"force", '\0', 0, G_OPTION_ARG_NONE, &force, N_("Ignoring any existing inhibitors"), NULL},
        {"no-prompt", '\0', 0, G_OPTION_ARG_NONE, &no_prompt, N_("Don't prompt for user confirmation"), NULL},
        {NULL}
};

static void
display_error (const char *message)
{
        g_printerr ("%s\n", message);
}

static DBusGConnection *
get_session_bus (void)
{
        DBusGConnection *bus;
        GError *error = NULL;

        bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

        if (bus == NULL) {
                g_warning ("Couldn't connect to session bus: %s", error->message);
                g_error_free (error);
        }

        return bus;
}

static DBusGProxy *
get_sm_proxy (void)
{
        DBusGConnection *connection;
        DBusGProxy      *sm_proxy;

        connection = get_session_bus ();
        if (connection == NULL) {
                display_error (_("Could not connect to the session manager"));
                return NULL;
        }

        sm_proxy = dbus_g_proxy_new_for_name (connection,
                                               GSM_SERVICE_DBUS,
                                               GSM_PATH_DBUS,
                                               GSM_INTERFACE_DBUS);

        if (sm_proxy == NULL) {
                display_error (_("Could not connect to the session manager"));
                return NULL;
        }

        return sm_proxy;
}

static void
do_logout (unsigned int mode)
{
        DBusGProxy *sm_proxy;
        GError     *error;
        gboolean    res;

        sm_proxy = get_sm_proxy ();
        if (sm_proxy == NULL) {
                return;
        }

        error = NULL;
        res = dbus_g_proxy_call (sm_proxy,
                                 "Logout",
                                 &error,
                                 G_TYPE_UINT, mode,
                                 G_TYPE_INVALID,
                                 G_TYPE_INVALID);

        if (!res) {
                if (error != NULL) {
                        g_warning ("Failed to call logout: %s",
                                   error->message);
                        g_error_free (error);
                } else {
                        g_warning ("Failed to call logout");
                }
        }

        if (sm_proxy != NULL) {
                g_object_unref (sm_proxy);
        }
}

static void
do_power_off (void)
{
        DBusGProxy *sm_proxy;
        GError     *error;
        gboolean    res;

        sm_proxy = get_sm_proxy ();
        if (sm_proxy == NULL) {
                return;
        }

        error = NULL;
        res = dbus_g_proxy_call (sm_proxy,
                                 "Shutdown",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_INVALID);

        if (!res) {
                if (error != NULL) {
                        g_warning ("Failed to call shutdown: %s",
                                   error->message);
                        g_error_free (error);
                } else {
                        g_warning ("Failed to call shutdown");
                }
        }

        if (sm_proxy != NULL) {
                g_object_unref (sm_proxy);
        }
}

int
main (int argc, char *argv[])
{
        GError *error;
        int     conflicting_options;

        /* Initialize the i18n stuff */
        bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, NULL, options, NULL, &error)) {
                g_warning ("Unable to start: %s", error->message);
                g_error_free (error);
                exit (1);
        }

        conflicting_options = 0;
        if (logout)
                conflicting_options++;
        if (power_off)
                conflicting_options++;
        if (conflicting_options > 1)
                display_error (_("Program called with conflicting options"));

        if (power_off) {
                do_power_off ();
        } else {
                /* default to logout */

                if (force)
                        do_logout (GSM_LOGOUT_MODE_FORCE);
                else if (no_prompt)
                        do_logout (GSM_LOGOUT_MODE_NO_CONFIRMATION);
                else
                        do_logout (GSM_LOGOUT_MODE_NORMAL);
        }

        return 0;
}