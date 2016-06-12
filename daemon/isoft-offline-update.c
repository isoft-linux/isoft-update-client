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
#include <systemd/sd-journal.h>
#include "rpm-helper.h"
#include "utils.h"

#include <packagekit-glib2/pk-progress-bar.h>

/* the trigger file that systemd uses to start a different boot target */
#define UPDATES_XML_FILENAME "updates.xml"

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
                write_debug_log ("Failed to set percentage for splash: %s",
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

static gboolean xmlNeedMerge(const char *src_xml)
{
    gboolean ret = FALSE;
    char *p = NULL;
    int  src_id = 0,dst_id = 0;

    if (!src_xml)
        return ret;

    write_debug_log("%s, line %d: [%s]cfg dir[%s]\n", __func__, __LINE__,src_xml,UPDATE_UPDATES_CONFDIR);
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
register_update_xml(GList *upt_list)
{
    GList *l = NULL;
    gchar *outdir = NULL;

    if (!g_file_test(UPDATE_UPDATES_CONFDIR, G_FILE_TEST_IS_DIR)) {
        write_debug_log("%s, line %d: mkdir[%s]", __func__, __LINE__, UPDATE_UPDATES_CONFDIR);
        g_mkdir_with_parents(UPDATE_UPDATES_CONFDIR, 0755);
    }

    write_debug_log("%s, line %d: %p", __func__, __LINE__, upt_list);
    for (l = upt_list; l; l = g_list_next(l)) {
        write_debug_log("%s, line %d: l->data[%s]", __func__, __LINE__, l->data);
        outdir = g_strndup(l->data, strlen(l->data));
        if (!outdir)
            continue;

        cp_update_xml(outdir);
        if (outdir) {
            g_free(outdir);
            outdir = NULL;
        }
    }

}

/*
* PK_OFFLINE_PKGS_PATH"/"PK_OFFLINE_PKGS_CFG_FILE
* ==
* /system-update-doing/.record.cfg
*
* no.0 file content:"update-2016-05-20-2-x86_64"
* no.1 created/appanded by daemon
* no.2 used and removed by offline-update
*/
static void remove_cfg_file()
{
    unlink(PK_OFFLINE_PKGS_PATH"/"PK_OFFLINE_PKGS_CFG_FILE);
    unlink(PK_OFFLINE_PKGS_PATH);
}

static bool
getUptActList(GList **upt_act_list)
{
    char record[1024]="";
    FILE *fd = fopen(PK_OFFLINE_PKGS_PATH"/"PK_OFFLINE_PKGS_CFG_FILE,"r");
    if (!fd) {
        return false;
    }
    while(fgets(record, 1000, fd))
    {
        record[1000] = '\0';
        int len = strlen(record);
        if (len < 10)
            continue;

        record[len-1] = '\0';

        // insert into list:
        *upt_act_list = g_list_append(*upt_act_list,g_strdup(record));

        memset(record,0,sizeof(record));
    }
    fclose(fd);

    return g_list_length(*upt_act_list) > 0 ? true:false;
}

static gboolean
pk_offline_update_do_update (PkProgressBar *progressbar, GError **perror)
{
    GList *rpm_list = NULL;
    g_progressbar = progressbar;
    char rpmfile[1024]="";

    g_result = -1;
    pk_progress_bar_title (g_progressbar, "Updating system...");
//#if 1
    // ln -s /var/cache/isoft-update /system-update(daemon)
    // move /system-update /system-update-doing(offline)
    // /var/cache/isoft-update/update-2016-05-20-2-x86_64 ==> /system-update-doing/update-2016-05-20-2-x86_64
    // outdir =[/var/cache/isoft-update/update-2016-05-20-2-x86_64]

    GDir *dir = NULL;
    GError *error = NULL;
    const gchar *rpm_filename = NULL;
    gchar *rpm_filepath = NULL;
    char *p = NULL;
    char upt_dir[1024]="";
    GList *upt_list=NULL;
    GList *upt_act_list=NULL;
    GList *l = NULL;
    bool getActList = false;
    getActList = getUptActList(&upt_act_list);
    if (!getActList) {
        write_error_log("no cfg file,can not do updation.");
        goto cleanup;
    }

    dir = g_dir_open(PK_OFFLINE_PKGS_PATH, 0, &error);
    if (!dir) {
        write_error_log("ERROR: failed to open %s,system update failed!", PK_OFFLINE_PKGS_PATH);
        return false;
    }
    while (rpm_filename = g_dir_read_name(dir)) {
        if (strcmp(rpm_filename,".") ==0 || strcmp(rpm_filename,"..") ==0 )
            continue;

        char rpms[PATH_MAX] = { '\0' };
        snprintf(rpms, sizeof(rpms) - 1, "%s/%s", PK_OFFLINE_PKGS_PATH, rpm_filename);
        if (g_file_test(rpms,G_FILE_TEST_IS_DIR)) {
            // rpm_filename must like update-2016-05-20-2-x86_64
            p = strrchr((char *)rpm_filename,'-');
            if (p) {
                if(strcasecmp(p,"-x86_64") != 0) {
                    continue;
                }
            } else
                continue;

            bool isInUptList = false;
            for (l = upt_act_list; l; l = g_list_next(l)) {
                if (strcmp((char *)l->data,(char *)rpm_filename) == 0) {
                        isInUptList = true;
                        break;
                }
            }
            if( !isInUptList ) {
                write_debug_log("%s, line %d: %s needs not update...", __func__, __LINE__, rpm_filename);
                continue;
            }
            memset(upt_dir,0,sizeof(upt_dir));
            snprintf(upt_dir,sizeof(upt_dir),"%s/%s", PK_OFFLINE_PKGS_PATH, rpm_filename);
            write_debug_log("%s, line %d: %s...\n", __func__, __LINE__, upt_dir);
            bool isExist = false;
            for (l = upt_list; l; l = g_list_next(l)) {
                if (strcmp(l->data,upt_dir) == 0) {
                    write_error_log("duplicate %s in upt_list,continue.", upt_dir);
                    isExist = true;
                }
            }
            if (!isExist) {
                upt_list = g_list_append(upt_list,g_strdup(upt_dir));
            }
        }
    }

    if (dir) {
        g_dir_close(dir);
        dir = NULL;
    }

    upt_list = g_list_sort(upt_list, upt_sort_func);

#if 1
    //snprintf(rpmfile,sizeof(rpmfile),"%s/*.rpm",PK_OFFLINE_PKGS_PATH);
    for (l = upt_list; l; l = g_list_next(l)) {
        gchar *outdir = NULL;
        gchar *packages_dir = NULL;
        gboolean  rpm_install_ok = false;
        char  upt_name[512] = "";
        char type[64]="";
        gchar *upt_xml_file = NULL;
        char cmd[1024] = { '\0' };
        gboolean ret;
        gint status;
        /* to write upt name to log files */
        rpm_install_ok = false;
        memset(upt_name,0,sizeof(upt_name));
        p = strrchr((char *)l->data,'/');
        if (p) {
            p++;
            // update-2015-09-10-5-x86_64.upt
            snprintf(upt_name,sizeof(upt_name),"%s.upt",p);
        }

        outdir = g_strndup(l->data, strlen(l->data));
        if (!outdir)
            goto next;

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
         * /var/cache/isoft-update/update-2016-05-20-2-x86_64/update/packages
         * upt[/var/cache/isoft-update/update-2016-05-20-2-x86_64.upt]
        */

        if (rpm_list) {
            g_list_free(rpm_list);
            rpm_list = NULL;
        }
        rpm_filename = NULL;
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

        //////////////// to call rpm install api ////////////////////////////

        write_debug_log ("will call rpm api.");
        install_rpm(rpm_list, rpm_progress_handle, NULL);
        write_debug_log ("call rpm api,done.");

        if (g_result == 0)
            rpm_install_ok = true;
        else {
            write_error_log("ERROR: failed to install/update pkgs in dir[%s]",packages_dir);
        }

        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"postscript",type);
        if (strcasecmp(type,"true") == 0) {
            memset(cmd, 0, sizeof(cmd));
            //snprintf(cmd, sizeof(cmd) - 1, "/usr/bin/sh %s/update/postscript", outdir);
            ret = g_spawn_command_line_sync(cmd, NULL, NULL, &status, &error);
            if (!ret) {
                write_error_log(_("ERROR: failed to run postscript"));
            }
        }
next:
        /*  after installation, write upt name to log file */
        strcpy(type,"unknown");
        get_xml_node_value(upt_xml_file,"type",type);
        char summary[512]="";
        get_xml_node_value(upt_xml_file,"summary",summary);
        char desc_buffer[512]="";
        get_xml_node_value(upt_xml_file,"description",desc_buffer);
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

    register_update_xml(upt_list);

cleanup:
    remove_cfg_file();

    if (upt_list) {
        g_list_foreach(upt_list, (GFunc)g_free, NULL);
        g_list_free(upt_list);
        upt_list = NULL;
    }

    if (rpm_list) {
        g_list_foreach(rpm_list, (GFunc)g_free, NULL);
        g_list_free(rpm_list);
        rpm_list = NULL;
    }
    if (upt_act_list) {
        g_list_foreach(upt_act_list, (GFunc)g_free, NULL);
        g_list_free(upt_act_list);
        upt_act_list = NULL;
    }

    return true;
#endif
#if 0
    snprintf(rpmfile,sizeof(rpmfile),"%s/*.rpm",PK_OFFLINE_PKGS_PATH);
    rpm_list = g_list_append(rpm_list, rpmfile);
    write_debug_log ("will call rpm api.");
    install_rpm(rpm_list, rpm_progress_handle, NULL);
    write_debug_log ("call rpm api,done.");
    g_list_free(rpm_list);
    rpm_list = NULL;
#endif
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

    /* ensure root user */
    if (getuid () != 0 || geteuid () != 0) {
            retval = EXIT_FAILURE;
            write_error_log("ERROR:This program can only be used using root!");
            goto out;
    }

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

