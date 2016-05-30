/*************************************************************************************
*  Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>                        *
*                                                                                   *
*  This program is free software; you can redistribute it and/or                    *
*  modify it under the terms of the GNU General Public License                      *
*  as published by the Free Software Foundation; either version 2                   *
*  of the License, or (at your option) any later version.                           *
*                                                                                   *
*  This program is distributed in the hope that it will be useful,                  *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
*  GNU General Public License for more details.                                     *
*                                                                                   *
*  You should have received a copy of the GNU General Public License                *
*  along with this program; if not, write to the Free Software                      *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
*************************************************************************************/

#include <stdio.h>
#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "update-generated.h"

typedef enum {
    STATUS_CHECK_UPDATE,
    STATUS_INSTALL_UPDATE
} Status;

static GMainLoop *m_loop = NULL;
static UpdateUpdate *m_update = NULL;

static void update_downloaded(UpdateUpdate *update, gchar *file) 
{
    gboolean result;

    printf(_("Downloaded: %s\n"), file);
    update_update_call_add_update_sync(update, file, &result, NULL, NULL);
}

static void errored(int error, gchar *details) 
{
}

static void finished(int status) 
{
    gboolean result;
    update_update_call_install_update_sync(m_update, &result, NULL, NULL);
}

int main(int argc, char *argv[]) 
{
    GError *error = NULL;
    gboolean result;

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, PROJECT_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_message(_("iSOFT Update Console"));

    m_update = update_update_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 
                                                    G_DBUS_PROXY_FLAGS_NONE, 
                                                    "org.isoftlinux.Update", 
                                                    "/org/isoftlinux/Update", 
                                                    NULL, 
                                                    &error);
    if (m_update == NULL) {
        g_error("%s", error->message);
        return -1;
    }

    g_object_connect(m_update, 
        "signal::update-downloaded", G_CALLBACK(update_downloaded), NULL,
        "signal::error", G_CALLBACK(errored), NULL,
        "signal::finished", G_CALLBACK(finished), NULL,
        NULL);

    update_update_call_check_update_sync(m_update, &result, NULL, &error);

    m_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(m_loop);

cleanup:
    if (m_loop) {
        g_main_loop_unref(m_loop);
        m_loop = NULL;
    }
    
    if (m_update) {
        g_object_unref(m_update);
        m_update = NULL;
    }

    if (error) {
        g_error_free(error);
        error = NULL;
    }

    return 0;
}
