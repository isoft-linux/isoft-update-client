/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2016 fj <fujiang@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <packagekit-glib2/pk-progress-bar.h>

/* the trigger file that systemd uses to start a different boot target */
#define PK_OFFLINE_TRIGGER_FILENAME "/system-update"
#define PK_OFFLINE_PKGS_PATH "/system-update-doing"

#include <systemd/sd-journal.h>
#include "rpm-helper.h"
#include "utils.h"

static gboolean
pk_offline_update_sigint_cb (gpointer user_data)
{
        sd_journal_print (LOG_WARNING, "Handling SIGINT");
        write_debug_log ("Handling SIGINT");
        return FALSE;
}


/**
 * pk_offline_update_set_plymouth_msg:
 **/
static void
pk_offline_update_set_plymouth_msg (const gchar *msg)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *cmdline = NULL;

    /* allow testing without sending commands to plymouth */
    if (g_getenv ("PK_OFFLINE_UPDATE_TEST") != NULL)
            return;
    cmdline = g_strdup_printf ("plymouth display-message --text=\"%s\"", msg);
    if (!g_spawn_command_line_async (cmdline, &error)) {
            sd_journal_print (LOG_WARNING,
                              "failed to display message on splash: %s",
                              error->message);
            write_debug_log ("failed to display message on splash: %s",
                             error->message);
    } else {
            sd_journal_print (LOG_INFO, "sent msg to plymouth '%s'", msg);
            //write_debug_log ("sent msg to plymouth '%s'", msg);
    }
}

/**
 * pk_offline_update_set_plymouth_mode:
 **/
static void
pk_offline_update_set_plymouth_mode (const gchar *mode)
{
    g_autoptr(GError) error = NULL;
    g_autofree gchar *cmdline = NULL;

    /* allow testing without sending commands to plymouth */
    if (g_getenv ("PK_OFFLINE_UPDATE_TEST") != NULL)
            return;
    cmdline = g_strdup_printf ("plymouth change-mode --%s", mode);
    if (!g_spawn_command_line_async (cmdline, &error)) {
            sd_journal_print (LOG_WARNING,
                              "failed to change mode for splash: %s",
                              error->message);
            write_debug_log ("Failed to change mode for splash: %s",
                             error->message);
    } else {
            sd_journal_print (LOG_INFO, "sent mode to plymouth '%s'", mode);
            write_debug_log ("sent mode to plymouth '%s'", mode);
    }
}

/**
 * pk_offline_update_set_plymouth_percentage:
 **/
static void
pk_offline_update_set_plymouth_percentage (guint percentage)
{
        g_autoptr(GError) error = NULL;
        g_autofree gchar *cmdline = NULL;

        /* allow testing without sending commands to plymouth */
        if (g_getenv ("PK_OFFLINE_UPDATE_TEST") != NULL)
                return;
        cmdline = g_strdup_printf ("plymouth system-update --progress=%i",
                                   percentage);
        if (!g_spawn_command_line_async (cmdline, &error)) {
                sd_journal_print (LOG_WARNING,
                                  "failed to set percentage for splash: %s",
                                  error->message);
        }
}

/**
 * pk_offline_update_progress_cb:
 **/
static void
pk_offline_update_progress_cb (//PkProgress *progress,
                               //PkProgressType type,
                               gpointer user_data)
{
#if 0
        PkInfoEnum info;
        PkProgressBar *progressbar = PK_PROGRESS_BAR (user_data);
        PkStatusEnum status;
        gint percentage;
        g_autofree gchar *msg = NULL;
        g_autoptr(PkPackage) pkg = NULL;

        switch (type) {
        case PK_PROGRESS_TYPE_ROLE:
                sd_journal_print (LOG_INFO, "assigned role");
                pk_progress_bar_start (progressbar, "Updating system");
                break;
        case PK_PROGRESS_TYPE_PACKAGE:

                g_object_get (progress, "package", &pkg, NULL);
                info = pk_package_get_info (pkg);
                if (info == PK_INFO_ENUM_UPDATING) {
                        msg = g_strdup_printf ("Updating %s",
                                               pk_package_get_name (pkg));
                        pk_progress_bar_start (progressbar, msg);
                } else if (info == PK_INFO_ENUM_INSTALLING) {
                        msg = g_strdup_printf ("Installing %s",
                                               pk_package_get_name (pkg));
                        pk_progress_bar_start (progressbar, msg);
                } else if (info == PK_INFO_ENUM_REMOVING) {
                        msg = g_strdup_printf ("Removing %s",
                                               pk_package_get_name (pkg));
                        pk_progress_bar_start (progressbar, msg);
                }
                sd_journal_print (LOG_INFO,
                                  "package %s\t%s-%s.%s (%s)",
                                  pk_info_enum_to_string (info),
                                  pk_package_get_name (pkg),
                                  pk_package_get_version (pkg),
                                  pk_package_get_arch (pkg),
                                  pk_package_get_data (pkg));

                break;
        case PK_PROGRESS_TYPE_PERCENTAGE:
                g_object_get (progress, "percentage", &percentage, NULL);
                if (percentage < 0)
                        return;
                sd_journal_print (LOG_INFO, "percentage %i%%", percentage);

                /* TRANSLATORS: this is the message we send plymouth to
                 * advise of the new percentage completion */
                msg = g_strdup_printf ("%s - %i%%", _("Installing Updates"), percentage);
                if (percentage > 10)
                        pk_offline_update_set_plymouth_msg (msg);

                /* print on terminal */
                pk_progress_bar_set_percentage (progressbar, percentage);

                /* update plymouth */
                pk_offline_update_set_plymouth_percentage (percentage);
                break;

        case PK_PROGRESS_TYPE_STATUS:
                g_object_get (progress, "status", &status, NULL);
                sd_journal_print (LOG_INFO,
                                  "status %s",
                                  pk_status_enum_to_string (status));

        default:
                break;
        }

#endif
}

/**
 * pk_offline_update_reboot:
 **/
static void
pk_offline_update_reboot (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = NULL;
    g_autoptr(GVariant) val = NULL;

    /* reboot using systemd */
    sd_journal_print (LOG_INFO, "rebooting");
    write_debug_log ("Rebooting");
    pk_offline_update_set_plymouth_mode ("shutdown");
    /* TRANSLATORS: we've finished doing offline updates */
    pk_offline_update_set_plymouth_msg (_("Rebooting after installing updates…"));
    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL) {
            sd_journal_print (LOG_WARNING,
                              "Failed to get system bus connection: %s",
                              error->message);
            write_debug_log ("Failed to get system bus connection: %s",
                             error->message);
            return;
    }
    val = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.systemd1",
                                       "/org/freedesktop/systemd1",
                                       "org.freedesktop.systemd1.Manager",
                                       "Reboot",
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);
    if (val == NULL) {
            sd_journal_print (LOG_WARNING,
                              "Failed to reboot: %s",
                              error->message);
            write_debug_log ("Failed to reboot: %s",
                             error->message);
            return;
    }

}

/**
 * pk_offline_update_power_off:
 **/
static void
pk_offline_update_power_off (void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) connection = NULL;
    g_autoptr(GVariant) val = NULL;

    /* reboot using systemd */
    sd_journal_print (LOG_INFO, "shutting down");
    write_debug_log ("shutting down");
    pk_offline_update_set_plymouth_mode ("shutdown");
    /* TRANSLATORS: we've finished doing offline updates */
    pk_offline_update_set_plymouth_msg (_("Shutting down after installing updates…"));
    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL) {
            sd_journal_print (LOG_WARNING,
                              "Failed to get system bus connection: %s",
                              error->message);
            write_debug_log ("Failed to get system bus connection: %s",
                             error->message);
            return;
    }
    val = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.systemd1",
                                       "/org/freedesktop/systemd1",
                                       "org.freedesktop.systemd1.Manager",
                                       "PowerOff",
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);
    if (val == NULL) {
            sd_journal_print (LOG_WARNING,
                              "Failed to power off: %s",
                              error->message);
            write_debug_log ("Failed to power off: %s",
                             error->message);
            return;
    }
}

/**
 * pk_offline_update_loop_quit_cb:
 **/
static gboolean
pk_offline_update_loop_quit_cb (gpointer user_data)
{
    GMainLoop *loop = (GMainLoop *) user_data;
    g_main_loop_quit (loop);
    return FALSE;
}

int g_result = -1;
PkProgressBar *g_progressbar = NULL;
static void pk_offline_update_show_percent(const char *filename,double progress)
{
    if(g_progressbar == NULL) {
        write_error_log("Progressbar not inited!");
        return;
    }

    int i =0;
    gint percentage = progress*100;
    char msg[512]="";
    const char *p = NULL;
    if( filename ==NULL || filename[0] ==0) {
        return;
    }
    p = strrchr(filename,'/');
    if(p == NULL) {
        p = filename;
    } else {
        p++;
    }
    pk_progress_bar_start (g_progressbar, p);

    /* TRANSLATORS: this is the message we send plymouth to
     * advise of the new percentage completion */
    snprintf (msg,sizeof(msg),"%s - %i%%", _("Installing Updates"), percentage);
    if (percentage > 10)
        pk_offline_update_set_plymouth_msg (msg);

    /* print on terminal */
    pk_progress_bar_set_percentage (g_progressbar, percentage);

    /* update plymouth */
    pk_offline_update_set_plymouth_percentage (percentage);

}

static void rpm_progress_handle(double *progress, void *arg_data, const char *filename)
{
    if ( fabs(*progress - 110.0) < 0.9) {
        write_error_log("Failed to install rpm.");
        g_result = -1;
    } else {
        pk_offline_update_show_percent(filename,*progress);
        g_result = 0;
    }
}

static gboolean
pk_offline_update_do_update (PkProgressBar *progressbar, GError **error)
{
    GList *rpm_list = NULL;
    g_progressbar = progressbar;
    char rpmfile[1024]="";

    pk_progress_bar_title (g_progressbar, "Updating system...");

    snprintf(rpmfile,sizeof(rpmfile),"%s/*.rpm",PK_OFFLINE_PKGS_PATH);
    rpm_list = g_list_append(rpm_list, rpmfile);
    write_debug_log ("will call rpm api.");
    install_rpm(rpm_list, rpm_progress_handle, NULL);
    write_debug_log ("call rpm api,done.");
    g_list_free(rpm_list);
    rpm_list = NULL;

    return TRUE;
}

int main(int argc,char *argv[])
{
    gint retval;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *link = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(PkProgressBar) progressbar = NULL;

    set_write_log_level(LOG_DEBUG);
    set_program_name("isoft-offline-update");
    write_debug_log ("System update...");
#if 0
    /* ensure root user */
    if (getuid () != 0 || geteuid () != 0) {
            retval = EXIT_FAILURE;
            write_error_log("ERROR:This program can only be used using root!");
            goto out;
    }
#endif
    /* verify this is pointing to our cache */
    link = g_file_read_link (PK_OFFLINE_TRIGGER_FILENAME, NULL);
    if (link == NULL) {
            write_debug_log ("No trigger, exiting");
            retval = EXIT_SUCCESS;
            printf("\ntrace:%s,%d\n",__FUNCTION__,__LINE__);
            goto out;
    }

    /* always do this first to avoid a loop if this tool segfaults */
    /* just rename to _doing, */
    g_rename (PK_OFFLINE_TRIGGER_FILENAME,PK_OFFLINE_PKGS_PATH);

    /* do stuff on ctrl-c */
    g_unix_signal_add_full (G_PRIORITY_DEFAULT,
                            SIGINT,
                            pk_offline_update_sigint_cb,
                            NULL,
                            NULL);

    /* use a progress bar when the user presses <esc> in plymouth */
    progressbar = pk_progress_bar_new ();
    pk_progress_bar_set_size (progressbar, 25);
    pk_progress_bar_set_padding (progressbar, 30);

    pk_offline_update_set_plymouth_mode ("updates");

    /* just update the system */
    if (!pk_offline_update_do_update ( progressbar, &error)) {
            retval = EXIT_FAILURE;
            //pk_offline_update_write_error (error);
            sd_journal_print (LOG_WARNING,
                              "failed to update system: %s",
                              error->message);
            write_error_log("ERROR:Failed to update system: %s",
                            error->message);
            goto out;
    }
    pk_progress_bar_end (progressbar);

    if (g_result == 0) {
        write_debug_log ("System update...done!");
    }
    retval = EXIT_SUCCESS;
out:
    if (g_result != 0) {
        write_debug_log ("System update...failed!");
    }

    /* if we failed, we pause to show any error on the screen */
    if (retval != EXIT_SUCCESS) {
            loop = g_main_loop_new (NULL, FALSE);
            g_timeout_add_seconds (10, pk_offline_update_loop_quit_cb, loop);
            g_main_loop_run (loop);
    }

    // test
    return retval;


    /* we have to manually either restart or shutdown */
    //if (action == PK_OFFLINE_ACTION_REBOOT)
            pk_offline_update_reboot ();
    //else if (action == PK_OFFLINE_ACTION_POWER_OFF)
    //        pk_offline_update_power_off ();
    return retval;

}

