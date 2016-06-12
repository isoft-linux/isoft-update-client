/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 * Copyright (C) 2015 fjiang <fujiang.zhu@i-soft.com.cn>
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

#include <stdlib.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <glib/gi18n.h>
#include <math.h>

#include <NetworkManager.h>

#include "daemon.h"
#include "download.h"
#include "archive.h"
#include "md5.h"
#include "rpm-helper.h"

#define VERSION_ID_PREFIX "VERSION_ID="
#define SERVER_PREFIX "Server = "
#define UPDATES_XML_FILENAME "updates.xml"
#define UPDATES_XML_XZ_FILENAME "updates.xml.xz"
#define UPDATES_XML_XZ_MD5SUM_FILENAME "updates.xml.xz.md5"
#define UPT_EXT ".upt"
#define MD5_HEX_SIZE 32

enum {
    PROP_0,
    PROP_DAEMON_VERSION,
    PROP_UPDATE_ENABLE,
    PROP_UPDATE_DURATION,
    PROP_AUTO_DOWNLOAD,
    PROP_AUTO_INSTALL
};

struct DaemonPrivate {
    GDBusConnection *bus_connection;
    GHashTable *extension_ifaces;
    GDBusMethodInvocation *context;

    GKeyFile *conf;
    gboolean update_enable;
    gchar *main_server;
    guint update_timer;
    gint64 update_duration;
    gboolean auto_download;
    gboolean auto_install;

    gchar *os_version;

    gboolean call_by_client;
    gboolean task_locked;

    gboolean updated_mirrorlist;
    GList *mirror_list;

    gchar *updates_md5sum;

    GList *upt_list;
    guint upt_count;
    guint downloaded_upt_count;
    gboolean need_reboot;
    gboolean install_ok;

    GDBusProxy *logind;
    GDBusProxy *nm;
};

#ifdef DLQUIT
pthread_t  g_all_pid[100];
static int g_pid_index = 0;
#endif
static void daemon_update_update_iface_init(UpdateUpdateIface *iface);
static void get_mirror_list(Daemon *daemon);
static void cancel_thread();
static void finish_check_update(gpointer user_data, gboolean call_by_client);
static gboolean daemon_check_update_timer(gpointer user_data);
static void parse_update_xml(Daemon *daemon, gchar *file_path);
static int merge_into_updates_xml(const char *src_xml, char action);

G_DEFINE_TYPE_WITH_CODE(Daemon, daemon, UPDATE_TYPE_UPDATE_SKELETON, G_IMPLEMENT_INTERFACE(UPDATE_TYPE_UPDATE, daemon_update_update_iface_init));

#define DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), TYPE_DAEMON, DaemonPrivate))

static const GDBusErrorEntry update_error_entries[] =
{
    { ERROR_FAILED, "org.isoftlinux.Update.Error.Failed" },
    { ERROR_PERMISSION_DENIED, "org.isoftlinux.Update.Error.PermissionDenied" },
    { ERROR_NOT_SUPPORTED, "org.isoftlinux.Update.Error.NotSupported" }
};

GQuark
error_quark()
{
    static volatile gsize quark_volatile = 0;

    g_dbus_error_register_error_domain("update_error",
                                       &quark_volatile,
                                       update_error_entries,
                                       G_N_ELEMENTS(update_error_entries));

    return (GQuark)quark_volatile;
}
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
error_get_type()
{
    static GType etype = 0;

    if (etype == 0) {
        static const GEnumValue values[] =
        {
            ENUM_ENTRY(ERROR_FAILED, "Failed"),
            ENUM_ENTRY(ERROR_PERMISSION_DENIED, "PermissionDenied"),
            ENUM_ENTRY(ERROR_NOT_SUPPORTED, "NotSupported"),
            { 0, 0, 0 }
        };
        g_assert(NUM_ERRORS == G_N_ELEMENTS(values) - 1);
        etype = g_enum_register_static("Error", values);
    }
    return etype;
}

static gchar *
get_os_version()
{
    FILE *stream = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read = -1;
    size_t offset = -1;
    char *tmp = NULL;

    stream = fopen("/etc/os-release", "r");
    if (stream == NULL)
        return NULL;

    while ((read = getline(&line, &len, stream)) != -1) {
        if (g_str_has_prefix(line, VERSION_ID_PREFIX)) {
            tmp = line;
            offset = strlen(VERSION_ID_PREFIX);
            while (offset--)
                tmp++;

            if (strlen(tmp) < 2)
                return NULL;

            return g_strndup(tmp, strlen(tmp) - 1);
        }
    }

    free(line);
    line = NULL;
    fclose(stream);
    stream = NULL;

    return NULL;
}

void rpm_progress_handle(double *progress, void *arg_data, const char *filename)
{
    Daemon *daemon = (Daemon *)arg_data;

    if ( fabs(*progress - 110.0) < 0.9) {

        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_INSTALL_UPDATE,
            g_strdup_printf(_("Failed to install rpm file")),
            ERROR_CODE_INSTALL);
        daemon->priv->install_ok = false;

        printf("%s, line %d: %f,Failed to install rpm file!\n", __func__, __LINE__,  *progress);
    } else {
        update_update_emit_percent_changed(UPDATE_UPDATE(daemon),
                                       STATUS_INSTALL_UPDATE,
                                       filename,
                                       *progress);

        daemon->priv->install_ok = true;
    }
}

static void
downloaded_mirrorlist(Daemon *daemon, gchar *file_path, gchar *url)
{
    write_debug_log("%s, line %d: %s, %s", __func__, __LINE__, file_path, url);
    daemon->priv->updated_mirrorlist = TRUE;
    get_mirror_list(daemon);
}

static void
download_mirrorlist(Daemon *daemon, gchar *server)
{
    DownloadData *data = NULL;
    GThread *thread = NULL;

    data = download_data_new();
    if (!data)
        return;

    data->daemon = g_object_ref(daemon);
    data->file_path = g_strdup_printf("%s/mirrorlist", UPDATE_CONFDIR);
    data->url = g_strdup_printf("%s/mirrorlist", server);
    data->handler = downloaded_mirrorlist;
#ifndef DLQUIT
    thread = g_thread_new(NULL, download_routine, data);
#else
    pthread_t pid;
    int isok = pthread_create(&pid,NULL , download_routine, data);
    g_all_pid[g_pid_index++] = pid;

    write_debug_log("%s, line %d: mirrorlist thread[%lu],return[%d]data[%p].", __func__, __LINE__,pid,isok,data);


#endif

    if (thread) {
        g_thread_unref(thread);
        thread = NULL;
    }
}

static void
get_mirror_list(Daemon *daemon)
{
    daemon->priv->task_locked = TRUE;

    FILE *stream = NULL;
    gchar *mirrorlist_path = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read = -1;
    size_t offset = -1;
    char *tmp = NULL;
    gchar *server = NULL;

    mirrorlist_path = g_strdup_printf("%s/mirrorlist", UPDATE_CONFDIR);
    if (!g_file_test(mirrorlist_path,
                     G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        download_mirrorlist(daemon, daemon->priv->main_server);
        return;
    }

    stream = fopen(mirrorlist_path, "r");
    if (stream == NULL)
        goto cleanup;

    while ((read = getline(&line, &len, stream)) != -1) {
        if (g_str_has_prefix(line, SERVER_PREFIX)) {
            tmp = line;
            offset = strlen(SERVER_PREFIX);
            while (offset--)
                tmp++;

            if (strlen(tmp) < 2)
                continue;

            server = g_strndup(tmp, strlen(tmp) - 1);
            write_debug_log("%s, line %d: get mirror list server:%s", __func__, __LINE__, server);
            daemon->priv->mirror_list = g_list_append(daemon->priv->mirror_list,
                                                      g_strdup(server));
            if (daemon->priv->updated_mirrorlist == FALSE)
                download_mirrorlist(daemon, server);
        }
    }
    write_debug_log("%s, line %d: get mirror list number:%d",
            __func__, __LINE__, g_list_length(daemon->priv->mirror_list));

cleanup:
    if (mirrorlist_path) {
        g_free(mirrorlist_path);
        mirrorlist_path = NULL;
    }

    if (stream) {
        fclose(stream);
        stream = NULL;
    }

    daemon->priv->task_locked = FALSE;
}

static void 
nm_signal(GDBusProxy *proxy, 
          gchar      *sender_name, 
          gchar      *signal_name, 
          GVariant   *parameters, 
          gpointer    user_data) 
{
    Daemon *daemon = (Daemon *)user_data;

    if (strcmp(signal_name, "StateChanged") != 0)
        return;

    GError *error = NULL;
    GVariant *state;
    NMState nm_state;
    state = g_dbus_proxy_call_sync (daemon->priv->nm, "state",
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE, -1,
                                    NULL, &error);
    if (error != NULL) {
       g_warning ("Failed to get network state: %s\n", error->message);
       g_error_free (error);
       return;
    }

    g_variant_get (state, "(u)", &nm_state);
    g_variant_unref (state);
    
    if(nm_state >= NM_STATE_CONNECTED_LOCAL) {
        cancel_thread();
        finish_check_update(daemon, FALSE);
    }
}

static void
daemon_init(Daemon *daemon)
{
    GError *error = NULL;

    daemon->priv = DAEMON_GET_PRIVATE(daemon);

    daemon->priv->extension_ifaces = daemon_read_extension_ifaces();

    daemon->priv->context = NULL;

    daemon->priv->conf = g_key_file_new();
    g_key_file_load_from_file(daemon->priv->conf,
                              PROJECT_CONF_FILE,
                              G_KEY_FILE_NONE,
                              &error);
    if (error) {
        write_error_log("ERROR: failed to load %s: %s", PROJECT_CONF_FILE, error->message);
        goto cleanup;
    }

    daemon->priv->os_version = get_os_version();

    daemon->priv->update_enable = g_key_file_get_boolean(daemon->priv->conf, 
                                                         "Daemon", 
                                                         "Enable", 
                                                         &error);
    if (error) {
        write_error_log("ERROR: failed to get update enable: %s", error->message);
        goto cleanup;
    }

    daemon->priv->main_server = g_key_file_get_string(daemon->priv->conf,
                                                      "Daemon",
                                                      "Server",
                                                      &error);
    if (error) {
        write_error_log("ERROR: failed to get main server: %s", error->message);
        goto cleanup;
    }

    daemon->priv->call_by_client = FALSE;
    daemon->priv->task_locked = FALSE;

    daemon->priv->updated_mirrorlist = FALSE;
    get_mirror_list(daemon);

    daemon->priv->update_duration = g_key_file_get_int64(daemon->priv->conf,
                                                         "Daemon",
                                                         "UpdateDuration",
                                                         &error);
    if (error) {
        write_error_log("ERROR: failed to get update duration: %s", error->message);
        goto cleanup;
    }
    daemon->priv->update_timer = g_timeout_add(daemon->priv->update_duration,
                                        (GSourceFunc)daemon_check_update_timer,
                                        daemon);

    daemon->priv->auto_download = g_key_file_get_boolean(daemon->priv->conf,
                                                         "Daemon",
                                                         "AutoDownload",
                                                         &error);
    write_debug_log("%s, line %d: get update_duration[%llu] from conf file.\n", __func__, __LINE__,daemon->priv->update_duration);

    if (error) {
        write_error_log("ERROR: failed to get AutoDownload: %s", error->message);
        goto cleanup;
    }

    daemon->priv->auto_install = g_key_file_get_boolean(daemon->priv->conf,
                                                        "Daemon",
                                                        "AutoInstall",
                                                        &error);
    if (error) {
        write_error_log("ERROR: failed to get AutoInstall: %s", error->message);
        goto cleanup;
    }

    daemon->priv->logind = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            NULL,
                                            "org.freedesktop.login1",
                                            "/org/freedesktop/login1",
                                            "org.freedesktop.login1.Manager",
                                            NULL,
                                            &error);
    if (error) {
        write_error_log("ERROR: failed to connect systemd logind: %s", error->message);
        goto cleanup;
    }

    daemon->priv->nm = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 
                                            G_DBUS_PROXY_FLAGS_NONE, 
                                            NULL, 
                                            "org.freedesktop.NetworkManager", 
                                            "/org/freedesktop/NetworkManager", 
                                            "org.freedesktop.NetworkManager", 
                                            NULL, 
                                            &error);
    if (daemon->priv->nm == NULL || error) {
        write_error_log("ERROR: failed to connect NetworkManager: %s", error->message);
    } else {
        g_signal_connect(daemon->priv->nm, "g-signal", G_CALLBACK(nm_signal), daemon);
    }

cleanup:
    if (error) {
        g_error_free(error);
        error = NULL;
    }
}

static void
daemon_finalize(GObject *object)
{
    Daemon *daemon;

    g_return_if_fail(IS_DAEMON(object));

    daemon = DAEMON(object);

    if (daemon->priv->bus_connection) {
        g_object_unref(daemon->priv->bus_connection);
        daemon->priv->bus_connection = NULL;
    }

    if (daemon->priv->os_version) {
        g_free(daemon->priv->os_version);
        daemon->priv->os_version = NULL;
    }

    if (daemon->priv->mirror_list) {
        g_list_foreach(daemon->priv->mirror_list, (GFunc)g_free, NULL);
        g_list_free(daemon->priv->mirror_list);
        daemon->priv->mirror_list = NULL;
    }

    if (daemon->priv->conf) {
        g_key_file_unref(daemon->priv->conf);
        daemon->priv->conf = NULL;
    }

    if (daemon->priv->updates_md5sum) {
        g_free(daemon->priv->updates_md5sum);
        daemon->priv->updates_md5sum = NULL;
    }

    if (daemon->priv->logind) {
        g_object_unref(daemon->priv->logind);
        daemon->priv->logind = NULL;
    }

    if (daemon->priv->nm) {
        g_object_unref(daemon->priv->nm);
        daemon->priv->nm = NULL;
    }

    G_OBJECT_CLASS(daemon_parent_class)->finalize(object);
}

static gboolean
register_update_daemon(Daemon *daemon)
{
    GError *error = NULL;

    daemon->priv->bus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (daemon->priv->bus_connection == NULL) {
        if (error != NULL) {
            write_log(LOG_CRIT,"error getting system bus: %s", error->message);
            g_error_free(error);
        }
        goto error;
    }

    if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(daemon),
                                          daemon->priv->bus_connection,
                                          "/org/isoftlinux/Update",
                                          &error)) {
        if (error != NULL) {
            write_log(LOG_CRIT,"error exporting interface: %s", error->message);
            g_error_free(error);
        }
        goto error;
    }

    return TRUE;

error:
    return FALSE;
}

Daemon *
daemon_new()
{
    Daemon *daemon;

    daemon = DAEMON(g_object_new(TYPE_DAEMON, NULL));

    if (!register_update_daemon(DAEMON(daemon))) {
        g_object_unref(daemon);
        goto error;
    }

    return daemon;

error:
    return NULL;
}

static void
throw_error(GDBusMethodInvocation *context,
            gint                   error_code,
            const gchar           *format,
            ...)
{
    va_list args;
    gchar *message;

    va_start(args, format);
    message = g_strdup_vprintf(format, args);
    va_end(args);

    g_dbus_method_invocation_return_error(context, ERROR, error_code, "%s", message);

    g_free(message);
}

static const gchar *
daemon_get_daemon_version(UpdateUpdate *object)
{
    return PROJECT_VERSION;
}

static gboolean
daemon_get_update_enable(UpdateUpdate *object)
{
    Daemon *daemon = (Daemon *)object;
    return daemon->priv->update_enable;
}

static gint64
daemon_get_update_duration(UpdateUpdate *object)
{
    Daemon *daemon = (Daemon *)object;
    return daemon->priv->update_duration;
}

static gboolean
daemon_get_auto_download(UpdateUpdate *object)
{
    Daemon *daemon = (Daemon *)object;
    return daemon->priv->auto_download;
}

static gboolean
daemon_get_auto_install(UpdateUpdate *object)
{
    Daemon *daemon = (Daemon *)object;
    return daemon->priv->auto_install;
}

GHashTable *
daemon_get_extension_ifaces(Daemon *daemon)
{
    return daemon->priv->extension_ifaces;
}

static void
daemon_get_property(GObject    *object,
                    guint       prop_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
    Daemon *daemon = DAEMON(object);
    switch (prop_id) {
    case PROP_DAEMON_VERSION:
        g_value_set_string(value, PROJECT_VERSION);
        break;

    case PROP_UPDATE_ENABLE:
        g_value_set_boolean(value, daemon->priv->update_enable);
        break;

    case PROP_UPDATE_DURATION:
        g_value_set_int64(value, daemon->priv->update_duration);
        break;

    case PROP_AUTO_DOWNLOAD:
        g_value_set_boolean(value, daemon->priv->auto_download);
        break;

    case PROP_AUTO_INSTALL:
        g_value_set_boolean(value, daemon->priv->auto_install);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
daemon_set_property(GObject      *object,
                    guint         prop_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_DAEMON_VERSION:
        g_assert_not_reached();
        break;

    case PROP_UPDATE_ENABLE:
        g_assert_not_reached();
        break;

    case PROP_UPDATE_DURATION:
        g_assert_not_reached();
        break;

    case PROP_AUTO_DOWNLOAD:
        g_assert_not_reached();
        break;

    case PROP_AUTO_INSTALL:
        g_assert_not_reached();
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
daemon_class_init(DaemonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = daemon_finalize;
    object_class->get_property = daemon_get_property;
    object_class->set_property = daemon_set_property;

    g_type_class_add_private(klass, sizeof(DaemonPrivate));

    g_object_class_override_property(object_class,
                                     PROP_DAEMON_VERSION,
                                     "daemon-version");
    g_object_class_override_property(object_class,
                                     PROP_UPDATE_ENABLE,
                                     "update-enable");
    g_object_class_override_property(object_class,
                                     PROP_UPDATE_DURATION,
                                     "update-duration");
    g_object_class_override_property(object_class,
                                     PROP_AUTO_DOWNLOAD,
                                     "auto-download");
    g_object_class_override_property(object_class,
                                     PROP_AUTO_INSTALL,
                                     "auto-install");
}

static gboolean
check_need_update(Daemon *daemon, gchar *latest, int *cur_latest)
{
    gchar *etc_updates_xml = NULL;
    gchar *updates_latest = NULL;
    gboolean ret = FALSE;
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;

    write_debug_log("%s, line %d: will check update.\n", __func__, __LINE__);

    if (!latest)
        goto cleanup;

    etc_updates_xml = g_strdup_printf("%s/%s",
                                      UPDATE_CONFDIR,
                                      UPDATES_XML_FILENAME);
    if (!g_file_test(etc_updates_xml,
                     G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        merge_into_updates_xml(NULL, 'r');
        return TRUE;
    }

    doc = xmlParseFile(etc_updates_xml);
    if (!doc) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse xml file %s"), etc_updates_xml),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to parse xml file %s", etc_updates_xml);

        goto cleanup;
    }
    cur = xmlDocGetRootElement(doc);
    if (!cur) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse xml file %s"), etc_updates_xml),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to get xml root node");

        goto cleanup;
    }
    updates_latest = (char*)xmlGetProp(cur, (const xmlChar *) "latest");
    if (latest && updates_latest) {
        *cur_latest = atoi(updates_latest);
        if (atoi(updates_latest) < atoi(latest))
            ret = TRUE;
    }

cleanup:
    if (etc_updates_xml) {
        g_free(etc_updates_xml);
        etc_updates_xml = NULL;
    }
    if (updates_latest) {
        g_free(updates_latest);
        updates_latest = NULL;
    }
    if (doc) {
        xmlFreeDoc(doc);
        doc = NULL;
    }

    return ret;
}

static void
check_upt_validate(Daemon *daemon, gchar *file_path, gchar *checksum)
{
    unsigned char md5num[16] = {'\0'};
    char md5sum[33] = {'\0'};

    if (!daemon || !file_path) {
        daemon->priv->task_locked = FALSE;
        return;
    }

    if (!checksum) {
        daemon->priv->task_locked = FALSE;
        write_error_log("ERROR: %s checksum is NULL", file_path);
        return;
    }

    if (md5_from_file(file_path, md5num) == -1) {
        write_error_log("ERROR: failed to get md5sum for %s", file_path);
        remove(file_path);

        daemon->priv->task_locked = FALSE;

        return;
    }

    md5_num2str(md5num, md5sum);
    if (g_strcmp0(checksum, md5sum) == 0) {
        GList *l = NULL;
        for (l = daemon->priv->upt_list; l; l = g_list_next(l)) {
            if (strcmp(l->data,file_path) == 0) {
                write_error_log("duplicate %s in list.", file_path);
                return;
            }
        }
        daemon->priv->upt_list = g_list_append(daemon->priv->upt_list,
                                               g_strdup(file_path));
    } else {
        remove(file_path);
        daemon->priv->upt_count--;
        write_error_log("upt checksum error,delete file: %s,md5sum:[%s]vs[%s]", file_path,checksum, md5sum);

        // check sum error:exit download routine.
        update_update_emit_finished(g_object_ref(UPDATE_UPDATE(daemon)),
                                    STATUS_INSTALL_UPDATE);

        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to check md5sum for file %s"), file_path),
            ERROR_CODE_DOWNLOAD);

        daemon->priv->task_locked = FALSE;

    }
}

// /var/cache/isoft-update/update-2015-08-27-1-x86_64.upt
static char *get_upt_id(gconstpointer src)
{
    char *p = strrchr((char *)src,'/');
    if( !p )
        return NULL;

    p = strchr(p,'-');
    if (!p)
        return NULL;
    p++;

    p = strchr(p,'-');
    if (!p)
        return NULL;
    p++;

    p = strchr(p,'-');
    if (!p)
        return NULL;
    p++;

    p = strchr(p,'-');
    if (!p)
        return NULL;
    p++;

    return p;
}

static int
upt_sort_func(gconstpointer a, gconstpointer b)
{
    int ret = 0,ida = 0,idb = 0;
    char *p = NULL;

    write_debug_log("%s, line %d: [%s] comp [%s]\n", __func__, __LINE__, a==NULL?"NULL":a, b==NULL?"null":b);

    if(!a || !b)
        return 0;

    p = get_upt_id(a);
    if (!p)
        return 0;

    ida = atoi(p);

    p = get_upt_id(b);
    if (!p)
        return 0;

    idb = atoi(p);

    if (ida > idb) {
        ret = 1;
    }
    else if (ida < idb) {
        ret = -1;
    }

    write_debug_log("%s, line %d: [%s] comp [%s],ret[%d]\n", __func__, __LINE__, a, b,ret);

    return ret;
}


static int do_update_xml()
{
    xmlDocPtr doc;
    xmlNodePtr dst;
    xmlNodePtr cur;
    xmlNodePtr nextcur;
    xmlChar* id = NULL;
    int ret = -1;
    int max_id = 0;

    doc = xmlReadFile(UPDATE_UPDATES_CONFDIR".xml", NULL, XML_PARSE_NOBLANKS);

    if( doc == NULL){
        write_debug_log("%s, line %d:Can not read xml file[%s]. \n",
                __func__, __LINE__,UPDATE_UPDATES_CONFDIR".xml");
        return -1;
    }

    cur = xmlDocGetRootElement(doc);
    if(cur == NULL){
        write_debug_log("%s, line %d:Xml file[%s] error. \n",
                __func__, __LINE__,UPDATE_UPDATES_CONFDIR".xml");
        xmlFreeDoc(doc);
        goto cleanup;
    }

    if(xmlStrcmp(cur->name, (const xmlChar *)"updates")){
        write_debug_log("%s, line %d:Xml file[%s] format error. \n",
                __func__, __LINE__,UPDATE_UPDATES_CONFDIR".xml");
        xmlFreeDoc(doc);
        goto cleanup;
    }
    dst = cur;
    ret = 0;
    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"update")) {
            nextcur = cur->xmlChildrenNode;
            while (nextcur != NULL) {
                if (!xmlStrcmp(nextcur->name, (const xmlChar *)"id")) {
                    id = xmlNodeListGetString(doc, nextcur->xmlChildrenNode, 1);

                    if (!id) {
                        break;
                    }
                    ret = atoi(id);
                    if (max_id < ret) {
                        max_id = ret;
                    }

                    xmlFree(id);
                    break;
                }

                nextcur = nextcur->next;
            }

        }
        cur = cur->next;
    }

    if (max_id > 0) {
        char sid[32]="";
        snprintf(sid,sizeof(sid),"%d",max_id);
        xmlSetProp(dst,"latest",sid);
        if (-1 == xmlSaveFormatFileEnc(UPDATE_UPDATES_CONFDIR".xml", doc, "utf-8", 1)) {
            write_debug_log("%s, line %d:To save Xml file[%s] error. \n",
                    __func__, __LINE__,UPDATE_UPDATES_CONFDIR".xml");
            goto cleanup;
        }
        ret = 0;
    }


cleanup:

    write_debug_log("%s, line %d: ret[%d],max_id[%d]", __func__, __LINE__,ret,max_id);

    return ret;
}

static gboolean xmlNeedMerge(const char *src_xml)
{
    gboolean ret = FALSE;
    char *p = NULL;
    int  src_id = 0,dst_id = 0;

    if (!src_xml)
        return ret;

    write_debug_log("%s, line %d: [%s]\n", __func__, __LINE__,src_xml);
    // /etc/update/updates/update-2015-08-27-1.xml
    p = get_upt_id(src_xml);
    if (!p)
        return ret;

    src_id = atoi(p);

    xmlDocPtr doc;
    xmlNodePtr cur;
    xmlChar* id = NULL;
    xmlNodePtr nextcur;
    gchar *updates_latest = NULL;
    gchar *file_path = g_strdup_printf("%s.xml", UPDATE_UPDATES_CONFDIR);
    if (!file_path)
        goto cleanup;

    doc = xmlParseFile(file_path);
    if (!doc) {
        write_error_log("ERROR: failed to parse xml file %s", file_path);
        goto cleanup;
    }

    cur = xmlDocGetRootElement(doc);
    if (!cur) {
        write_error_log("ERROR: failed to get xml file[%s] root node",file_path);
        goto cleanup;
    }

    updates_latest = (char*)xmlGetProp(cur, (const xmlChar *) "latest");
    if (updates_latest) {
        if (atoi(updates_latest) < src_id) {
            ret = TRUE;
            goto cleanup;
        }

    }

    if(xmlStrcmp(cur->name, (const xmlChar *)"updates")){
        write_debug_log("%s, line %d:Xml file[%s] format error. \n",
                __func__, __LINE__,UPDATE_UPDATES_CONFDIR".xml");
        goto cleanup;
    }
    cur = cur->xmlChildrenNode;
    gboolean isEnd = FALSE;
    while (cur != NULL && isEnd) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"update")) {
            nextcur = cur->xmlChildrenNode;
            while (nextcur != NULL) {
                if (!xmlStrcmp(nextcur->name, (const xmlChar *)"id")) {
                    id = xmlNodeListGetString(doc, nextcur->xmlChildrenNode, 1);

                    if (!id) {
                        break;
                    }
                    dst_id = atoi(id);
                    if (src_id == dst_id) {
                        ret  = FALSE;
                        isEnd = TRUE;
                    }

                    xmlFree(id);
                    break;
                }

                nextcur = nextcur->next;
            }

        }
        cur = cur->next;
    }

cleanup:
    write_debug_log("%s, line %d: [%s]\n", __func__, __LINE__,ret?"need update xml":"do not update xml");
    if (file_path) {
        g_free(file_path);
        file_path = NULL;
    }

    if (updates_latest) {
        g_free(updates_latest);
        updates_latest = NULL;
    }

    if (doc) {
        xmlFreeDoc(doc);
        doc = NULL;
    }

    return ret;
}

static int do_merge_into_updates_xml(const char *src_xml,char action)
{
    int ret = -1;
    FILE *fpsrc = NULL;
    FILE *fpdst = NULL;
    long file_size = -1;
    long pos = -1,len = -1;
    char buf[512]="";
    if (!src_xml)
        return -1;

    if (action == 'a' && !xmlNeedMerge(src_xml)) {
        return 0;
    }

    if (NULL == (fpdst = fopen(UPDATE_UPDATES_CONFDIR".xml", "rb+"))) {
        write_debug_log("%s, line %d: open file[%s] error",
                __func__, __LINE__, UPDATE_UPDATES_CONFDIR".xml");
        return -1;
    }

    fseek(fpdst, 0, SEEK_END);
    file_size = ftell(fpdst);

    rewind(fpdst);
    pos = file_size > 500 ? 500 : file_size/2;
    fseek(fpdst,0-pos,SEEK_END);

    if (fread(buf, sizeof(char), sizeof(buf)-1, fpdst) < 1) {
        goto cleanup;
    }

    char *p = strstr(buf,"</updates>");
    if (!p) {
        goto cleanup;
    }

    pos = strlen(p);
    rewind(fpdst);
    fseek(fpdst,0-pos,SEEK_END);

    if (NULL == (fpsrc = fopen(src_xml, "rb"))) {
        write_debug_log("%s, line %d: open file[%s] error",
                __func__, __LINE__, src_xml);
        goto cleanup;
    }

    memset(buf,0,sizeof(buf));
    if (fread(buf, sizeof(char), sizeof(buf)-1, fpsrc) < 1) {
        goto cleanup;
    }

    p = strstr(buf,"<update>");
    if (!p) {
        goto cleanup;
    }
    *p = '\0';
    pos = strlen(buf);
    rewind(fpsrc);
    fseek(fpsrc,pos,SEEK_SET);

    write_debug_log("%s, line %d: pos[%ld]",
            __func__, __LINE__, pos);

    strcpy(buf,"\n");

    fwrite(buf, 1, strlen(buf), fpdst);

    len = 1;
    memset(buf,0,sizeof(buf));
    pos = 0;
    while (len > 0) {
        len = fread(buf, 1, sizeof(buf), fpsrc);
        if (len > 0) {
            pos = 1;
            fwrite(buf, 1, len, fpdst);
        }
        memset(buf,0,sizeof(buf));
    }
    if (pos >0) {
        strcpy(buf,"\n</updates>\n");
        fwrite(buf, 1, strlen(buf), fpdst);
    }

    ret = 0;

cleanup:
    if (fpsrc)
        fclose(fpsrc);
    if (fpdst)
        fclose(fpdst);

    if (ret == 0) {
        do_update_xml();
    }

    return ret;
}


/**
 * merge_into_updates_xml:
 *
 * to create updates.xml or merge @src_xml into updates.xml
 *
 * @src_xml: xml file,allways in /etc/update/updates/update-2015.8.26-1.xml.
 * @action: 'r'--recreate /etc/update/updates.xml by
 *              /etc/update/updates/update-<date>-<id>.xml, @src_xml is unuseful.
 *          'a'--only merge @src_xml into/etc/update/updates.xml
 *
 * Returns: 0--ok,others error.
 */
static int merge_into_updates_xml(const char *src_xml,char action)
{
    int ret = 0;
    FILE *fpdst = NULL;
    char buf[512]="";
    GDir *xml_dir = NULL;
    const gchar *xml_filename = NULL;
    GList *rpm_list = NULL;
    GError *error = NULL;
    gchar *xml_filepath = NULL;
    GList *l = NULL;

    if (action != 'r' && action != 'a')
        return -1;

    if (action == 'r' ) {

        // no.1 create updates.xml
        if (NULL == (fpdst = fopen(UPDATE_UPDATES_CONFDIR".xml", "wb"))) {
            write_debug_log("%s, line %d: open file[%s] error",
                    __func__, __LINE__, UPDATE_UPDATES_CONFDIR".xml");
            return -1;
        }

        snprintf(buf,sizeof(buf),"%s","<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
        size_t write_size = strlen(buf);

        if (fwrite(buf, 1, write_size, fpdst)!= write_size) {
            write_debug_log("%s, line %d:Write error: %s\n",
                    __func__, __LINE__,strerror(errno));
            goto cleanup;
        }

        snprintf(buf,sizeof(buf),"%s","<updates latest=\"0\">\n</updates>\n");
        write_size = strlen(buf);
        if (fwrite(buf, 1, write_size, fpdst)!= write_size) {
            write_debug_log("%s, line %d:Write error: %s\n",
                    __func__, __LINE__,strerror(errno));
            goto cleanup;
        }
        fclose(fpdst);
        fpdst = NULL;

        // no.2 get all update-<date>-<id>.xml
        xml_dir = g_dir_open(UPDATE_UPDATES_CONFDIR, 0, &error);
        if (!xml_dir) {
            write_error_log("ERROR: failed to open dir %s,error info[%s]",
                      UPDATE_UPDATES_CONFDIR,strerror(errno));
            goto cleanup;
        }
        ret = 0;
        while (xml_filename = g_dir_read_name(xml_dir)) {
            if (strcmp(UPDATES_XML_FILENAME,(char *)xml_filename) == 0) {
                continue;
            }
            if (strstr((char *)xml_filename,"update-") == NULL) {
                continue;
            }
            xml_filepath = g_strdup_printf("%s/%s", UPDATE_UPDATES_CONFDIR, xml_filename);
            write_debug_log("%s, line %d:get xml[%s]\n",
                    __func__, __LINE__,xml_filepath);
            rpm_list = g_list_append(rpm_list, xml_filepath);
            ret ++;
        }

        if (ret) {
            ret = 0;
            rpm_list = g_list_sort(rpm_list, upt_sort_func);
            for (l = rpm_list; l; l = g_list_next(l)) {
                if (strlen(l->data)  < 5)
                    continue;

                write_debug_log("%s, line %d:get xml 2[%s]\n",
                        __func__, __LINE__,l->data);
                do_merge_into_updates_xml((char *)l->data,'r');
            }

            g_list_foreach(rpm_list, (GFunc)g_free, NULL);

        }

    } // no.3 merge every .xml into updtes.xml
    else {
        if (src_xml == NULL)
            return -1;

        do_merge_into_updates_xml(src_xml,'a');
    }

cleanup:
    if (fpdst)
        fclose(fpdst);

    if (xml_dir) {
        g_dir_close(xml_dir);
        xml_dir = NULL;
    }

    if (error)
        g_error_free(error);

    return ret;

}

/**
 * cp_update_xml:
 *
 * to copy /var/cache/isoft-update/update-<date>-<id>-x86_64/update/update.xml
 * to /etc/update/updates/.
 *
 * alse merge update.xml into /etc/update/updates/updates.xml by merge_into_updates_xml().
 *
 * @outdir: root path like /var/cache/isoft-update/update-<date>-<id>-x86_64.
 *
 * Returns: 0--ok,others error.
 */
static int cp_update_xml(gchar *outdir)
{
    GError *error = NULL;
    gchar * xml_name = NULL;
    gchar *source = NULL;
    gchar *destination = NULL;
    int ret = -1;

    if (!outdir)
        return -1;

    source = g_strdup_printf("%s/update/update.xml", outdir);
    if (!source)
        goto cleanup;

    char *p = strrchr((char *)outdir,'/');
    if (!p)
        goto cleanup;

    p++;
    xml_name = g_strdup_printf("%s", p);
    if (!xml_name)
        goto cleanup;
    char *end = strrchr((char*)xml_name,'-');
    if (!end)
        goto cleanup;

    *end = '\0';

    destination = g_strdup_printf("%s/%s.xml", UPDATE_UPDATES_CONFDIR,xml_name);

    write_debug_log("%s, line %d: %s", __func__, __LINE__, source);
    link(source, destination);

    merge_into_updates_xml(destination,'a');

cleanup:
    if (source) {
        g_free(source);
        source = NULL;
    }
    if (xml_name) {
        g_free(xml_name);
        xml_name = NULL;
    }
    if (destination) {
        g_free(destination);
        destination = NULL;
    }
    if (error) {
        g_error_free(error);
        error = NULL;
    }

    return ret;
}


static void
register_update_xml(Daemon *daemon)
{
    GList *l = NULL;
    gchar *outdir = NULL;

    if (!g_file_test(UPDATE_UPDATES_CONFDIR, G_FILE_TEST_IS_DIR)) {
        write_debug_log("%s, line %d: mkdir[%s]", __func__, __LINE__, UPDATE_UPDATES_CONFDIR);
        g_mkdir_with_parents(UPDATE_UPDATES_CONFDIR, 0755);
    }

    write_debug_log("%s, line %d: %p", __func__, __LINE__, daemon->priv->upt_list);
    for (l = daemon->priv->upt_list; l; l = g_list_next(l)) {
        if (strlen(l->data) - strlen(UPT_EXT) < 1)
            continue;

        write_debug_log("%s, line %d: l->data[%s]", __func__, __LINE__, l->data);

        outdir = g_strndup(l->data, strlen(l->data) - strlen(UPT_EXT));
        if (!outdir)
            continue;

        cp_update_xml(outdir);
        if (outdir) {
            g_free(outdir);
            outdir = NULL;
        }
    }

}

static void
inhibit_install_done(GObject        *source_object,
                     GAsyncResult   *res,
                     gpointer       user_data)
{
    Daemon *daemon = (Daemon *)user_data;
    int fd = -1;
    GError *error = NULL;
    GVariant *data = NULL;
    GList *l = NULL;
    gchar *outdir = NULL;
    gchar *packages_dir = NULL;
    GDir *dir = NULL;
    const gchar *rpm_filename = NULL;
    gchar *rpm_filepath = NULL;
    GList *rpm_list = NULL;
    gboolean  rpm_install_ok = false;
    char  upt_name[512] = "";
    char type[64]="";
    gchar *upt_xml_file = NULL;
    gboolean  install_ok = true;
    char cmd[1024] = { '\0' };
    gboolean ret;
    gint status;

    data = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);
    if (data == NULL || error) {
        write_error_log("ERROR: failed to inhibit: %s", error->message);
        goto cleanup;
    }
    g_variant_get(data, "(h)", &fd);

    write_debug_log("%s, line %d: %d", __func__, __LINE__, daemon->priv->upt_list);
    // TODO: duplicate data in upt_list(autodownload && not autoinstall) + (manual check)
    daemon->priv->upt_list = g_list_sort(daemon->priv->upt_list, upt_sort_func);

    /* new routine */
#if 1
    status = symlink(DOWNLOAD_DIR,PK_OFFLINE_TRIGGER_FILENAME);
    if(status !=0 ){
        // maybe run it two times
        write_error_log("failed to create symlink for dir[%s],error info[%s]",DOWNLOAD_DIR, strerror(errno));
        //return ;
    }

    FILE *fd_cfg = fopen(PK_OFFLINE_TRIGGER_FILENAME"/"PK_OFFLINE_PKGS_CFG_FILE,"a+");
    if (!fd_cfg) {
        write_error_log("failed to open cfg file[%s]",PK_OFFLINE_TRIGGER_FILENAME"/"PK_OFFLINE_PKGS_CFG_FILE);
        return ;
    }

    for (l = daemon->priv->upt_list; l; l = g_list_next(l)) {
        if (strlen(l->data) - strlen(UPT_EXT) < 1)
            continue;

        outdir = g_strndup(l->data, strlen(l->data) - strlen(UPT_EXT));
        if (!outdir)
            continue;

        if (archive_unpack(l->data, outdir) == -1) {
            write_error_log("ERROR: failed to unpack upt file %s", l->data);
            remove(l->data);
            if (outdir) {
                g_free(outdir);
                outdir = NULL;
            }
            continue;
        }

        /* after unpack upt, create symbolic link ,then continue.*/
        const char *p = strrchr((char *)outdir,'/');
        if (p) {
            p++;
            fputs(p,fd_cfg);
            fputs("\n",fd_cfg);

        } else {
            write_error_log("ERROR: outdir[%s] is invalid.", outdir);
        }

        if (outdir) {
            g_free(outdir);
            outdir = NULL;
        }
    }

    fclose(fd_cfg);
    fd_cfg = NULL;

    if (data) {
        g_variant_unref(data);
        data = NULL;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    return ;

#endif

    for (l = daemon->priv->upt_list; l; l = g_list_next(l)) {

        /* to write upt name to log files */
        rpm_install_ok = false;
        memset(upt_name,0,sizeof(upt_name));
        char *p = strrchr((char *)l->data,'/');
        if (p) {
            p++;
            snprintf(upt_name,sizeof(upt_name),"%s",p);
        }

        if (strlen(l->data) - strlen(UPT_EXT) < 1)
            goto next;

        outdir = g_strndup(l->data, strlen(l->data) - strlen(UPT_EXT));
        if (!outdir)
            goto next;

        if (archive_unpack(l->data, outdir) == -1) {
            write_error_log("ERROR: failed to unpack upt file %s", l->data);
            remove(l->data);
            goto next;
        }

        /* to get type by update.xml */
        upt_xml_file = g_strdup_printf("%s/update/update.xml", outdir);

        packages_dir = g_strdup_printf("%s/update/packages", outdir);
        write_debug_log("%s, line %d: %s,upt[%s]\n", __func__, __LINE__, packages_dir,l->data);
        dir = g_dir_open(packages_dir, 0, &error);
        if (!dir) {
            write_error_log("ERROR: failed to open %s", packages_dir);
            goto next;
        }
        /*
         * outdir =[/var/cache/isoft-update/update-2016-05-20-2-x86_64]
         * => ln -s /var/cache/isoft-update /system-update
         *
        2016-05-25 15:04:32 (isoft-update-daemon:24815): DEBUG: inhibit_install_done, line 1552:
                                                         /var/cache/isoft-update/update-2016-05-20-2-x86_64/update/packages
                                                         ,upt[/var/cache/isoft-update/update-2016-05-20-2-x86_64.upt]
        */

        if (rpm_list) {
            g_list_free(rpm_list);
            rpm_list = NULL;
        }

        while (rpm_filename = g_dir_read_name(dir)) {
            char rpms[PATH_MAX] = { '\0' };
            snprintf(rpms, sizeof(rpms) - 1, "%s/%s", packages_dir, rpm_filename);
            if (g_file_test(rpms,
                            G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
                rpm_filepath = g_strdup_printf("%s/%s", packages_dir, rpm_filename);
                write_debug_log("%s, line %d: %s...\n", __func__, __LINE__, rpm_filepath);
                rpm_list = g_list_append(rpm_list, rpm_filepath);
                break; // only get path
            }
        }
        if (!rpm_list) {
            goto next;
        }
        // prescript & postscript
        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"prescript",type);
        if (strcasecmp(type, "true") == 0) {
            snprintf(cmd, sizeof(cmd) - 1, "/usr/bin/sh %s/update/prescript", outdir);
            ret = g_spawn_command_line_sync(cmd, NULL, NULL, &status, &error);
            if (!ret) {
                write_error_log(_("ERROR: failed to run prescript"));
            }
        }

        daemon->priv->install_ok = false;
        install_rpm(rpm_list, rpm_progress_handle, daemon);
        rpm_install_ok = daemon->priv->install_ok;
        if (!rpm_install_ok)
            install_ok = false;

        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"postscript",type);
        if (strcasecmp(type,"true") == 0) {
            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd) - 1, "/usr/bin/sh %s/update/postscript", outdir);
            ret = g_spawn_command_line_sync(cmd, NULL, NULL, &status, &error);
            if (!ret) {
                write_error_log(_("ERROR: failed to run postscript"));
            }
        }

        // to get reboot flag
        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"needreboot",type);
        if (strcasecmp(type,"true") == 0) {
            daemon->priv->need_reboot = true;
        }
next:
        /*  after installation, write upt name to log file */
        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"type",type);
        char summary[512]="";
        get_xml_node_value(upt_xml_file,"summary",summary);
        char desc_buffer[512]="";
        get_xml_node_value(upt_xml_file,"description",desc_buffer); // use <description lang="zh_CN">
        write_debug_log("%s, line %d: by[%s] get type[%s]\n", __func__, __LINE__, upt_xml_file,type);
        write_upt_reocord_file(upt_name,type,'i',rpm_install_ok?1:0,summary,desc_buffer);

        if (dir) {
            g_dir_close(dir);
            dir = NULL;
        }

        if (packages_dir) {
            g_free(packages_dir);
            packages_dir = NULL;
        }

        if (outdir) {
            g_free(outdir);
            outdir = NULL;
        }

        if (error) {
            g_error_free(error);
            error = NULL;
        }
    }

    register_update_xml(daemon);

    g_list_foreach(daemon->priv->upt_list, (GFunc)g_free, NULL);
    g_list_free(daemon->priv->upt_list);
    daemon->priv->upt_list = NULL;

    update_update_complete_install_update(NULL, daemon->priv->context, TRUE);

    daemon->priv->task_locked = FALSE;

    update_update_emit_finished(g_object_ref(UPDATE_UPDATE(daemon)),
                                STATUS_INSTALL_UPDATE);

    //install_ok = true;

    // emit reboot?
    if (install_ok) {
        if (daemon->priv->need_reboot) {
            update_update_emit_need_reboot(UPDATE_UPDATE(daemon),
                                           daemon->priv->need_reboot);
            printf("DEBUG: %s, line %d: emit reboot.\n",__func__, __LINE__);
        }
        else
            printf("DEBUG: %s, line %d: do not emit reboot.\n",__func__, __LINE__);
    }

    printf("DEBUG: %s, line %d: [do not emit reboot]%s].\n",__func__, __LINE__,install_ok?"install_ok":"install_ failed");
cleanup:
printf("DEBUG: %s, line %d: [do not emit reboot]%s].\n",__func__, __LINE__,install_ok?"install_ok":"install_ failed");
    if (!install_ok) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                    ERROR_INSTALL_UPDATE,
                    g_strdup_printf(_("Failed to install rpm packages")),
                    ERROR_CODE_INSTALL);
    }

    if (daemon->priv->task_locked == TRUE)
        daemon->priv->task_locked = FALSE;

    if (error) {
        g_error_free(error);
        error = NULL;
    }

    if (data) {
        g_variant_unref(data);
        data = NULL;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

static void
install_update_packages(Daemon *daemon)
{
    if (daemon->priv->task_locked) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_TASK_LOCKED,
                                 _("Task locked"),
                                 ERROR_CODE_OTHERS);
        write_error_log("%s, line %d:Task locked.",__func__, __LINE__);
    }
    daemon->priv->task_locked = TRUE;

    g_dbus_proxy_call(daemon->priv->logind,
                      "Inhibit",
                      g_variant_new("(ssss)",
                                    "shutdown:handle-power-key:handle-hibernate-key",
                                    _("iSOFT Update Daemon"),
                                    _("iSOFT Update Daemon Installer in Progress"),
                                    "block"),
                      G_DBUS_CALL_FLAGS_NONE,
                      -1,
                      NULL,
                      (GAsyncReadyCallback)inhibit_install_done,
                      daemon);

}

static gboolean
daemon_install_update(UpdateUpdate *update, GDBusMethodInvocation *context)
{
    Daemon *daemon = (Daemon*)update;
    daemon->priv->context = context;
    install_update_packages(daemon);
    write_debug_log("%s, line %d:.",__func__, __LINE__);
    return TRUE;
}

static void
downloaded_update_upt_error(Daemon      *daemon,
                            gchar       *file_path,
                            gchar       *url,
                            CURLcode    error)
{
    daemon->priv->upt_count--;
    daemon->priv->task_locked = FALSE;
}

static void
downloaded_update_upt_with_checksum(Daemon    *daemon,
                                    gchar     *file_path,
                                    gchar     *url,
                                    gchar     *checksum)
{
    daemon->priv->downloaded_upt_count++;

    check_upt_validate(daemon, file_path, checksum);

    if (daemon->priv->upt_count == daemon->priv->downloaded_upt_count) {
        update_update_complete_check_update(NULL, daemon->priv->context, TRUE);
        if (!daemon->priv->auto_install || daemon->priv->call_by_client) {
            update_update_emit_finished(g_object_ref(UPDATE_UPDATE(daemon)),
                                        STATUS_DOWNLOAD_UPDATE);
        }

        write_debug_log("%s, line %d:[%s]--[%s].",__func__, __LINE__,
                        daemon->priv->call_by_client?"call by c":"not by c",
                        daemon->priv->auto_install?"auto install":"not auto install");

        if (!daemon->priv->call_by_client && daemon->priv->auto_install)
            install_update_packages(daemon);

        daemon->priv->task_locked = FALSE;
    }
}

static gboolean
daemon_download_update(UpdateUpdate             *update,
                       GDBusMethodInvocation    *context,
                       const gchar              *file) 
{
    Daemon *daemon = (Daemon *)update;
    parse_update_xml(daemon, (gchar *)file);
    return TRUE;
}

static gboolean
daemon_add_update(UpdateUpdate          *update,
                  GDBusMethodInvocation *context,
                  const gchar           *file)
{
    Daemon *daemon = (Daemon *)update;

    daemon->priv->call_by_client = TRUE;

    GList *l = NULL;
    for (l = daemon->priv->upt_list; l; l = g_list_next(l)) {
        if (strcmp(l->data,file) == 0) {
            write_error_log("duplicate %s in list2.", file);
            return TRUE;
        }
    }

    daemon->priv->upt_list = g_list_append(daemon->priv->upt_list,
                                           g_strdup(file));
    return TRUE;
}

static void
downloaded_update_upt_progress(Daemon   *daemon,
                               gdouble  percent,
                               gchar    *file_path)
{
    update_update_emit_percent_changed(UPDATE_UPDATE(daemon),
                                       STATUS_DOWNLOAD_UPDATE,
                                       file_path,
                                       percent);
}

static gboolean
parse_update(xmlDocPtr  doc,
             xmlNodePtr cur,
             Daemon     *daemon,
             gchar      *updates_latest, 
             int        cur_latest)
{
    gboolean ret = FALSE;
    gchar *id = NULL;
    gchar *arch = NULL;
    gchar *date = NULL;
    gchar *checksum = NULL;
    gchar *output = NULL;
    gboolean res;
    int status = -1;
    GError *error = NULL;
    gchar *file_path = NULL;
    DownloadData *data = NULL;
    GThread *thread = NULL;
    gchar *type = NULL;
    char  upt_name[512]="";
    gchar *summary = NULL;
    char desc_buffer[512]="";

    cur = cur->xmlChildrenNode;
    while (cur) {
        if (!xmlStrcmp(cur->name, (const xmlChar *) "id"))
            id = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (!xmlStrcmp(cur->name, (const xmlChar *) "arch"))
            arch = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (!xmlStrcmp(cur->name, (const xmlChar *) "date"))
            date = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (!xmlStrcmp(cur->name, (const xmlChar *) "checksum"))
            checksum = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (!xmlStrcmp(cur->name, (const xmlChar *) "type"))
            type = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (!xmlStrcmp(cur->name, (const xmlChar *) "summary"))
            summary = (gchar*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

        if (strcmp(cur->name, (const xmlChar *) "description") ==0) {
            memset(desc_buffer,0,sizeof(desc_buffer));
            get_desc_node(doc,cur,desc_buffer);

        }

        cur = cur->next;
    }

    if (id!=NULL && arch!=NULL && date!=NULL && checksum!=NULL && type!=NULL)
        write_debug_log("%s, line %d: %s %s %s %s,type[%s]\n",
            __func__, __LINE__, id, arch, date, checksum,type);

    if (!id || !updates_latest) {
        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    if (atoi(id) < 1 || atoi(id) > atoi(updates_latest)) {
        write_error_log("ERROR: invalid update id %s", id);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    /* 不重复下载 id 比 /etc/update/updates.xml latest 小的 upt 包 */
    if (atoi(id) <= cur_latest)
        goto cleanup;

    res = g_spawn_command_line_sync("/usr/bin/uname -m",
                                    &output,
                                    NULL,
                                    &status,
                                    &error);
    if (!res) {
        write_error_log("ERROR: failed to get arch");

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    write_debug_log("%s, line %d: %s", __func__, __LINE__, output);
    if (strstr(output, arch) == NULL) {
        write_error_log("ERROR: invalid update arch %s", arch);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    // send type for icon
    if (type && strcmp(type,"security")== 0) {
        write_debug_log("%s, line %d: %s", __func__, __LINE__, type);
        update_update_emit_type_changed(g_object_ref(UPDATE_UPDATE(daemon)),
                                TYPE_SECURITY);
    }
    else {
        write_debug_log("%s, line %d: %s", __func__, __LINE__, type);
        update_update_emit_type_changed(g_object_ref(UPDATE_UPDATE(daemon)),
                                TYPE_NORMAL);
    }

    file_path = g_strdup_printf("%s/update-%s-%s-%s%s", 
                                DOWNLOAD_DIR, 
                                date, 
                                id, 
                                arch,
                                UPT_EXT);
    if (g_file_test(file_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        daemon->priv->upt_count++;
        downloaded_update_upt_with_checksum(daemon, file_path, NULL, checksum);
        ret = FALSE;
        write_debug_log("%s, line %d: file[%s] has already exist.", __func__, __LINE__,file_path);
        goto cleanup;
    }

    data = download_data_new();
    if (!data) {
        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    /*  download a .upt, then write upt name to log file */
    snprintf(upt_name,sizeof(upt_name),"update-%s-%s-%s%s",
             date,
             id,
             arch,
             UPT_EXT);

    write_upt_reocord_file(upt_name,type,'u',-1,summary,desc_buffer);

    ret = TRUE;
    data->daemon = g_object_ref(daemon);
    data->file_path = g_strdup_printf("%s/update-%s-%s-%s%s",
                                      DOWNLOAD_DIR,
                                      date,
                                      id,
                                      arch,
                                      UPT_EXT);
    data->url = g_strdup_printf("%s/%s/%s/update-%s-%s-%s%s",
                                daemon->priv->main_server,
                                daemon->priv->os_version,
                                arch,
                                date,
                                id,
                                arch,
                                UPT_EXT);
    write_debug_log("%s, line %d: will dl[%s].", __func__, __LINE__, data->url);
    data->checksum = checksum;
    data->handler_with_checksum = downloaded_update_upt_with_checksum;
    data->handler_error = downloaded_update_upt_error;
    data->handler_progress = downloaded_update_upt_progress;
#ifndef DLQUIT
    thread = g_thread_new(NULL, download_routine, data);
#else
    pthread_t pid;
    int isok = pthread_create(&pid,NULL , download_routine, data);
    write_debug_log("%s, line %d: parse update thread[%lu],return[%d]data[%p].", __func__, __LINE__,pid,isok,data);
    g_all_pid[g_pid_index++] = pid;
    // pthread_detach(pid); // coredump

#endif
cleanup:
    if (error) {
        g_error_free(error);
        error = NULL;
    }

    if (file_path) {
        g_free(file_path);
        file_path = NULL;
    }

    if (thread) {
        g_thread_unref(thread);
        thread = NULL;
    }

    return ret;
}

static void
parse_update_xml(Daemon *daemon, gchar *file_path)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    gchar *updates_latest = NULL;
    int cur_latest = 0;

    doc = xmlParseFile(file_path);
    if (!doc) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse xml file %s"), file_path),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to parse xml file %s", file_path);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    cur = xmlDocGetRootElement(doc);
    if (!cur) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse xml file %s"), file_path),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to get xml root node");

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    updates_latest = (char*)xmlGetProp(cur, (const xmlChar *) "latest");
    write_debug_log("%s, line %d: %s", __func__, __LINE__, updates_latest);
    if (!check_need_update(daemon, updates_latest, &cur_latest)) {
        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    daemon->priv->downloaded_upt_count = 0;
    daemon->priv->upt_count = 0;
    daemon->priv->need_reboot = false;
    daemon->priv->install_ok = false;
    cur = cur->xmlChildrenNode;
    while (cur) {
        if (xmlStrcmp(cur->name, "update") == 0) {
            if (parse_update(doc, cur, daemon, updates_latest, cur_latest))
                daemon->priv->upt_count++;
        }

        cur = cur->next;
    }
    write_debug_log("%s, line %d, get upt num:%d from file[%s]",
            __func__, __LINE__, daemon->priv->upt_count,file_path);

cleanup:
    if (doc) {
        xmlFreeDoc(doc);
        doc = NULL;
    }
}

static void
downloaded_update_xml_tarball(Daemon    *daemon,
                              gchar     *file_path,
                              gchar     *url)
{
    unsigned char md5num[16] = {'\0'};
    char md5sum[33] = {'\0'};
    gchar *updates_xml_path = NULL;
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;
    gchar *updates_latest = NULL;
    GList *l = NULL;
    int cur_latest = 0;
    int upt_latest = 0;

    if (md5_from_file(file_path, md5num) == -1) {
        write_error_log("failed to get md5sum for %s", file_path);
        update_update_emit_error(UPDATE_UPDATE(daemon),
                    ERROR_CHECK_UPDATE,
                    g_strdup_printf(_("Failed to get md5sum file")),
                    ERROR_CODE_DOWNLOAD);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    md5_num2str(md5num, md5sum);
    write_debug_log("%s, line %d: %s", __func__, __LINE__, md5sum);
    if (g_strcmp0(daemon->priv->updates_md5sum, md5sum) != 0) {
        write_error_log("md5sum %s is wrong", file_path);
        update_update_emit_error(UPDATE_UPDATE(daemon),
                    ERROR_CHECK_UPDATE,
                    g_strdup_printf(_("md5sum %s is wrong"), file_path),
                    ERROR_CODE_DOWNLOAD);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    updates_xml_path = g_strdup_printf("%s/%s",
                                       DOWNLOAD_DIR,
                                       UPDATES_XML_FILENAME);
    if (xz_decompress(file_path, updates_xml_path) == -1) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                    ERROR_CHECK_UPDATE,
                    g_strdup_printf(_("Failed to decompress %s"), file_path),
                    ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to decompress %s", file_path);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    doc = xmlParseFile(updates_xml_path);
    if (!doc) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse updates xml file %s"),
                updates_xml_path),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to parse xml file %s", updates_xml_path);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    cur = xmlDocGetRootElement(doc);
    if (!cur) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
            ERROR_CHECK_UPDATE,
            g_strdup_printf(_("Failed to parse xml file %s"), updates_xml_path),
            ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to get xml root node");

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    updates_latest = (char*)xmlGetProp(cur, (const xmlChar *) "latest");
    write_debug_log("%s, line %d: %s,[%s][%s]", __func__, __LINE__,
            updates_latest,
            daemon->priv->auto_download?"auto dl":"not dl",
            daemon->priv->call_by_client?"call by c":"not by c");
    if (!check_need_update(daemon, updates_latest, &cur_latest)) {
        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    if (!daemon->priv->auto_download || daemon->priv->call_by_client) {
    //if (!(daemon->priv->auto_download || daemon->priv->call_by_client) ) {
        update_update_emit_check_update_finished(UPDATE_UPDATE(daemon),
                                                 updates_xml_path, 
                                                 cur_latest);

        write_debug_log("%s, line %d:check update[%s]cur_latest[%d]", __func__, __LINE__,updates_xml_path,cur_latest);

        goto cleanup;
    }

    daemon->priv->downloaded_upt_count = 0;
    daemon->priv->upt_count = 0;
    daemon->priv->need_reboot = false;
    daemon->priv->install_ok = false;
    cur = cur->xmlChildrenNode;
    while (cur) {
        if (xmlStrcmp(cur->name, "update") == 0) {
            if (parse_update(doc, cur, daemon, updates_latest, cur_latest))
                daemon->priv->upt_count++;
        }

        cur = cur->next;
    }
    write_debug_log("%s, line %d, upt_count: %d",
            __func__, __LINE__, daemon->priv->upt_count);

cleanup:

    if (updates_latest) {
        upt_latest = atoi(updates_latest);
        if (cur_latest >= upt_latest) {
            write_debug_log("%s, line %d,the current version is newer than server, do not update it.[%d]vs[%d]",
                    __func__, __LINE__,cur_latest,upt_latest);
        }
        else
            write_debug_log("%s, line %d,it is need to update.", __func__, __LINE__);
    }
    else {
        write_debug_log("%s, line %d,update error.", __func__, __LINE__);
    }
    if (updates_xml_path) {
        g_free(updates_xml_path);
        updates_xml_path = NULL;
    }

    if (doc) {
        xmlFreeDoc(doc);
        doc = NULL;
    }
}

static void
downloaded_update_xml_md5sum(Daemon *daemon,
                             gchar  *file_path,
                             gchar  *url)
{
    GThread *thread = NULL;
    DownloadData *data = NULL;
    FILE *md5sum_file = NULL;
    long file_size = 0;
    char *buf = NULL;

    md5sum_file = fopen(file_path, "r");
    if (!md5sum_file) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                        ERROR_CHECK_UPDATE,
                        g_strdup_printf(_("Failed to open %s"), file_path),
                        ERROR_CODE_DOWNLOAD);

        write_error_log("ERROR: failed to open %s", file_path);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    fseek(md5sum_file, 0, SEEK_END);
    file_size = ftell(md5sum_file);
    if (file_size == 0) {
        // curl CURLE_COULDNT_CONNECT, don't emit error
        #if 0
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_CHECK_UPDATE,
                                 g_strdup_printf(_("%s is empty"), file_path));
        #endif

        write_error_log("%s is empty", UPDATES_XML_XZ_MD5SUM_FILENAME);

        daemon->priv->task_locked = FALSE;

        ////?????????// todo 20151030
        goto cleanup;
    }
    rewind(md5sum_file);
    buf = malloc(sizeof(char) * file_size);
    if (!buf) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_CHECK_UPDATE,
                                 g_strdup(_("Failed to allocate memory")),
                                 ERROR_CODE_OTHERS);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    memset(buf, 0, sizeof(char) * file_size);
    if (fread(buf, sizeof(char), file_size, md5sum_file) != file_size) {
        write_error_log("ERROR: failed to read md5sum");

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }
    daemon->priv->updates_md5sum = g_strndup(buf, MD5_HEX_SIZE);
    write_debug_log("%s, line %d: %s", __func__, __LINE__, daemon->priv->updates_md5sum);

    data = download_data_new();
    if (!data) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_CHECK_UPDATE,
                                 g_strdup(_("Failed to allocate memory")),
                                 ERROR_CODE_OTHERS);

        daemon->priv->task_locked = FALSE;

        goto cleanup;
    }

    data->daemon = g_object_ref(daemon);
    data->file_path = g_strdup_printf("%s/%s",
                                      DOWNLOAD_DIR,
                                      UPDATES_XML_XZ_FILENAME);
    data->url = g_strdup_printf("%s/%s/%s",
                                daemon->priv->main_server,
                                daemon->priv->os_version,
                                UPDATES_XML_XZ_FILENAME);
    data->handler = downloaded_update_xml_tarball;
#ifndef DLQUIT
    thread = g_thread_new(NULL, download_routine, data);
#else
    pthread_t pid;
    int isok = pthread_create(&pid,NULL , download_routine, data);
    write_debug_log("%s, line %d: xml md5sum thread[%lu],return[%d]data[%p].", __func__, __LINE__,pid,isok,data);
    g_all_pid[g_pid_index++] = pid;

#endif
cleanup:
    if (md5sum_file) {
        fclose(md5sum_file);
        md5sum_file = NULL;
    }

    if (buf) {
        free(buf);
        buf = NULL;
    }

    if (thread) {
        g_thread_unref(thread);
        thread = NULL;
    }
}

static void
downloaded_update_xml_md5sum_error(Daemon   *daemon,
                                   gchar    *file_path,
                                   gchar    *url,
                                   CURLcode error)
{
#if 0 // do it in upper routine;
    // network error
    if (error != CURLE_COULDNT_CONNECT) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                             ERROR_CHECK_UPDATE,
                             g_strdup_printf(_("Failed to download %s"), url));
    }
#endif
    daemon->priv->task_locked = FALSE;
}

static void
finish_check_update(gpointer user_data, gboolean call_by_client)
{
    Daemon *daemon = (Daemon *)user_data;
    GThread *thread = NULL;
    DownloadData *data = NULL;

    if (daemon->priv->task_locked) {
#if 0 // 20151030 while downloading, other task allways go here
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_TASK_LOCKED,
                                 g_strdup(_("Task locked")),
                                 ERROR_CODE_OTHERS);
#endif
        write_error_log("%s, line %d:Task locked.",__func__, __LINE__);
        return;
    }
    daemon->priv->call_by_client = call_by_client;
    daemon->priv->task_locked = TRUE;

    data = download_data_new();
    if (!data) {
        update_update_emit_error(UPDATE_UPDATE(daemon),
                                 ERROR_CHECK_UPDATE,
                                 g_strdup(_("Failed to allocate memory")),
                                 ERROR_CODE_OTHERS);

        daemon->priv->task_locked = FALSE;
        return;
    }

    data->daemon = g_object_ref(daemon);
    data->file_path = g_strdup_printf("%s/%s",
                                      DOWNLOAD_DIR,
                                      UPDATES_XML_XZ_MD5SUM_FILENAME);
    data->url = g_strdup_printf("%s/%s/%s",
                                daemon->priv->main_server,
                                daemon->priv->os_version,
                                UPDATES_XML_XZ_MD5SUM_FILENAME);
    data->handler = downloaded_update_xml_md5sum;
    data->handler_error = downloaded_update_xml_md5sum_error;
#ifndef DLQUIT
    thread = g_thread_new(NULL, download_routine, data);
#else
    pthread_t pid;
    int isok = pthread_create(&pid,NULL , download_routine, data);
    write_debug_log("%s, line %d: finish check update thread[%lu],return[%d]data[%p].", __func__, __LINE__,pid,isok,data);
    //pthread_detach(pid);
    g_all_pid[g_pid_index++] = pid;
#endif
    if (thread) {
        g_thread_unref(thread);
        thread = NULL;
    }
}

static gboolean
daemon_check_update_timer(gpointer user_data)
{
    finish_check_update(user_data, FALSE);
    return TRUE;
}

static gboolean
daemon_check_update(UpdateUpdate *update, GDBusMethodInvocation *context)
{
    Daemon *daemon = (Daemon *)update;
    daemon->priv->context = context;
    finish_check_update(daemon, TRUE);
    return TRUE;
}

/* TODO: 未实现
 * 终止所有线程
 */
static void
cancel_thread()
{
#ifdef DLQUIT
    int i = 0;
    pthread_t pid;
    int ret = 0;
    for (i = 0;i<g_pid_index;i++) {
        pid = g_all_pid[i];
        g_all_pid[i] = 0;
        ret  = pthread_kill(pid,0);
        if(ret != ESRCH) {
            write_debug_log("%s, line %d: will kill this thread[%lu][%d].", __func__, __LINE__,pid,ret);
            ret = pthread_kill(pid, SIGQUIT);
            write_debug_log("%s, line %d: will kill this thread[%lu],ret[%d][%s].", __func__, __LINE__,pid,ret,strerror(errno));
        }
        else
            write_debug_log("%s, line %d: no this thread[%lu][%d].", __func__, __LINE__,pid,ret);

    }
    g_pid_index = 0;
#endif
}

static gboolean
daemon_set_update_enable(UpdateUpdate          *update,
                         GDBusMethodInvocation *context,
                         gboolean              enable)
{
    Daemon *daemon = (Daemon *)update;
    if (daemon->priv->update_enable == enable)
        return TRUE;

    daemon->priv->update_enable = enable;
    g_key_file_set_boolean(daemon->priv->conf,
                           "Daemon",
                           "Enable",
                           daemon->priv->update_enable);
    if (daemon->priv->update_enable == FALSE) {
        g_source_remove(daemon->priv->update_timer);
        cancel_thread();
    } else {
        daemon->priv->update_timer = g_timeout_add(
                                        daemon->priv->update_duration,
                                        (GSourceFunc)daemon_check_update_timer,
                                        daemon);
    }
    update_update_emit_update_enable_changed(UPDATE_UPDATE(daemon),
                                             daemon->priv->update_enable);
    write_debug_log("%s, line %d: set update enable flag to %s.", __func__, __LINE__,enable?"true":"false");

    return TRUE;
}

static gboolean
daemon_set_update_duration(UpdateUpdate             *update,
                           GDBusMethodInvocation    *context,
                           gint64                   duration)
{
    Daemon *daemon = (Daemon *)update;
    if (daemon->priv->update_duration == duration)
        return TRUE;

    daemon->priv->update_duration = duration;
    g_key_file_set_int64(daemon->priv->conf,
                         "Daemon",
                         "UpdateDuration",
                         daemon->priv->update_duration);
    g_source_remove(daemon->priv->update_timer);
    cancel_thread();
    daemon->priv->update_timer = g_timeout_add(daemon->priv->update_duration,
                                    (GSourceFunc)daemon_check_update_timer,
                                    daemon);

    write_debug_log("%s, line %d: set update_duration to %llu.", __func__, __LINE__,duration);

    return TRUE;
}

static gboolean
daemon_set_auto_download(UpdateUpdate           *update,
                         GDBusMethodInvocation  *context,
                         gboolean               autoable)
{
    Daemon *daemon = (Daemon *)update;
    if (daemon->priv->auto_download == autoable)
        return TRUE;

    daemon->priv->auto_download = autoable;
    g_key_file_set_boolean(daemon->priv->conf,
                         "Daemon",
                         "AutoDownload",
                         daemon->priv->auto_download);
    write_debug_log("%s, line %d: set auto_download flag  to %s.", __func__, __LINE__,autoable?"true":"false");

    return TRUE;
}

static gboolean
daemon_set_auto_install(UpdateUpdate            *update,
                        GDBusMethodInvocation   *context,
                        gboolean                autoable)
{
    Daemon *daemon = (Daemon *)update;
    if (daemon->priv->auto_install == autoable)
        return TRUE;

    daemon->priv->auto_install = autoable;
    g_key_file_set_boolean(daemon->priv->conf,
                         "Daemon",
                         "AutoInstall",
                         daemon->priv->auto_install);

    write_debug_log("%s, line %d: set AutoInstall flag  to %s.", __func__, __LINE__,autoable?"true":"false");

    return TRUE;
}

static gboolean
daemon_save_conf_file(UpdateUpdate            *update,
                      GDBusMethodInvocation   *context)
{
    Daemon *daemon = (Daemon *)update;
    GError *error = NULL;

    write_debug_log("%s, line %d: will save conf file.", __func__, __LINE__);

    g_key_file_save_to_file(daemon->priv->conf,
                              PROJECT_CONF_FILE,
                              &error);
    if (error) {
        write_error_log("ERROR: failed to save %s: %s", PROJECT_CONF_FILE, error->message);
        return FALSE;
    }

    return TRUE;

}
static gboolean daemon_clean_upt_record()
{
    int ret = -1,ret2 = -1;

    write_debug_log("%s, line %d: will clean updated record file.", __func__, __LINE__);
    ret = unlink(INSTALLED_LOG_FILE);
    ret2 = unlink(NOT_INSTALLED_LOG_FILE);
    if (ret != 0 && ret2!=0) {
        write_error_log("ERROR: failed to delete file %s and %s", INSTALLED_LOG_FILE,NOT_INSTALLED_LOG_FILE);
        return FALSE;
    }

    return TRUE;
}

static void
daemon_update_update_iface_init(UpdateUpdateIface *iface)
{
    iface->get_daemon_version = daemon_get_daemon_version;
    iface->get_update_enable = daemon_get_update_enable;
    iface->get_update_duration = daemon_get_update_duration;
    iface->get_auto_download = daemon_get_auto_download;
    iface->get_auto_install = daemon_get_auto_install;
    iface->handle_set_update_enable = daemon_set_update_enable;
    iface->handle_set_update_duration = daemon_set_update_duration;
    iface->handle_set_auto_download = daemon_set_auto_download;
    iface->handle_set_auto_install = daemon_set_auto_install;
    iface->handle_check_update = daemon_check_update;
    iface->handle_download_update = daemon_download_update;
    iface->handle_add_update = daemon_add_update;
    iface->handle_install_update = daemon_install_update;
    iface->handle_save_conf_file = daemon_save_conf_file;
    iface->handle_clean_upt_record = daemon_clean_upt_record;
}
