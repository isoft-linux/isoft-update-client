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

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmts.h>
#include <glib/gi18n.h>
#include "rpm-helper.h"
#include "utils.h"

enum modes {
    MODE_QUERY      = (1 <<  0),
    MODE_VERIFY     = (1 <<  3),
#define MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL    = (1 <<  1),
    MODE_ERASE      = (1 <<  2),
#define MODES_IE (MODE_INSTALL | MODE_ERASE)

    MODE_UNKNOWN    = 0
};

#define MODES_FOR_NODEPS    (MODES_IE | MODE_VERIFY)
#define MODES_FOR_TEST      (MODES_IE)

static int quiet;
static void *m_arg_data = NULL;

static struct poptOption optionsTable[] = {
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
     N_("Query/Verify package selection options:"),
     NULL},
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
     N_("Query options (with -q or --query):"),
     NULL},
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
     N_("Verify options (with -V or --verify):"),
     NULL},
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
     N_("Install/Upgrade/Erase options:"),
     NULL},
    {"quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, &quiet, 0, NULL, NULL},
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
     N_("Common options for all rpm modes and executables:"),
     NULL},
    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

static void (*g_progress_handle)(double *progress, void *arg_data,const char *filename);

static int rpmcliProgressState = 0;
static int rpmcliPackagesTotal = 0;
static int rpmcliHashesCurrent = 0;
static int rpmcliHashesTotal = 0;
static int rpmcliProgressCurrent = 0;
static int rpmcliProgressTotal = 0;
void * rpmShowProgress(const void * arg,
                        const rpmCallbackType what,
                        const rpm_loff_t amount,
                        const rpm_loff_t total,
                        fnpyKey key,
                        void * data)
{
    Header h = (Header) arg;
    int flags = (int) ((long)data);
    void * rc = NULL;
    const char * filename = (const char *)key;
    static FD_t fd = NULL;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
        if (filename == NULL || filename[0] == '\0')
            return NULL;
        fd = Fopen(filename, "r.ufdio");
        if(fd == NULL) {
            printf("%s,%d: open [%s]error\n",__FUNCTION__,__LINE__,filename);
        }
        /* FIX: still necessary? */
        if (fd == NULL || Ferror(fd)) {
            if (fd != NULL) {
                Fclose(fd);

                fd = NULL;
            }
        } else
            fd = fdLink(fd);
        return (void *)fd;
        break;

    case RPMCALLBACK_INST_CLOSE_FILE:
        /* FIX: still necessary? */
        fd = fdFree(fd);
        if (fd != NULL) {
            Fclose(fd);
            fd = NULL;
        }
        break;

    case RPMCALLBACK_INST_START:
    case RPMCALLBACK_UNINST_START:
        if (rpmcliProgressState != what) {
            rpmcliProgressState = what;
            if (flags & INSTALL_HASH) {
                if (what == RPMCALLBACK_INST_START) {
                    fprintf(stdout, _("Updating / installing...\n"));
                } else {
                    fprintf(stdout, _("Cleaning up / removing...\n"));
                }
                fflush(stdout);
            }
        }

        rpmcliHashesCurrent = 0;
        if (h == NULL || !(flags & INSTALL_LABEL))
            break;
        if (flags & INSTALL_HASH) {
            char *s = headerGetAsString(h, RPMTAG_NEVR);
            if (isatty (STDOUT_FILENO))
                fprintf(stdout, "%4d:%-33.33s", rpmcliProgressCurrent + 1, s);
            else
                fprintf(stdout, "%-38.38s", s);
            (void) fflush(stdout);
            free(s);
        } else {
            char *s = headerGetAsString(h, RPMTAG_NEVRA);
            fprintf(stdout, "%s\n", s);
            (void) fflush(stdout);
            free(s);
        }
        break;

    case RPMCALLBACK_INST_STOP:
        break;

    case RPMCALLBACK_TRANS_PROGRESS:
    case RPMCALLBACK_INST_PROGRESS:
    case RPMCALLBACK_UNINST_PROGRESS:
        if (flags & INSTALL_PERCENT) {
            double progress = (double) (total ? (((float) amount) / total): 100.0);
            g_progress_handle(&progress, m_arg_data,filename);
#if 0
            usleep(100000);
    #if 1
            printf("\nin show progress: total[%d] amount[%d]-->[%f]filename[%s]\n",
                (int) total,(int)amount,
                (double) (total ? ((((float) amount) / total) * 100): 100.0),filename);
    #endif
#endif
        }
        else if (flags & INSTALL_HASH)
            //printHash(amount, total);
        (void) fflush(stdout);

        break;

    case RPMCALLBACK_TRANS_START:
        rpmcliHashesCurrent = 0;
        rpmcliProgressTotal = 1;
        rpmcliProgressCurrent = 0;
        rpmcliPackagesTotal = total;
        rpmcliProgressState = what;
        if (!(flags & INSTALL_LABEL))
            break;
        if (flags & INSTALL_HASH)
            fprintf(stdout, "%-38s", _("Preparing..."));
        else
            fprintf(stdout, "%s\n", _("Preparing packages..."));
        (void) fflush(stdout);
        break;

    case RPMCALLBACK_TRANS_STOP:
        //if (flags & INSTALL_HASH)
            //printHash(1, 1);    /* Fixes "preparing..." progress bar */
        rpmcliProgressTotal = rpmcliPackagesTotal;
        rpmcliProgressCurrent = 0;
        break;

    case RPMCALLBACK_UNINST_STOP:
        break;
    case RPMCALLBACK_UNPACK_ERROR:
        break;

    case RPMCALLBACK_CPIO_ERROR:
        break;
    case RPMCALLBACK_SCRIPT_ERROR:
        break;
    case RPMCALLBACK_SCRIPT_START:
        break;
    case RPMCALLBACK_SCRIPT_STOP:
        break;
    case RPMCALLBACK_UNKNOWN:
    default:
        break;
    }

    return rc;
}


static void do_install(gpointer data, gpointer user_data)
{
    if (!data)
        return;
    char path[1024]="";
    char *pstr = NULL;
    snprintf(path,sizeof(path),"%s",data);
    pstr = strrchr(path,'/');
    if (pstr == NULL) {
        write_debug_log("%s, line %d:path[%s] error!\n", __func__, __LINE__, path);
        return;
    }
    *pstr='\0';
    strcat(path,"/*.rpm");

    //g_print("%s %d:file[%s]\n",__FUNCTION__,__LINE__,data);
    write_debug_log("%s, line %d: will install:%s\n", __func__, __LINE__, data);

    rpmts ts = NULL;
    struct rpmInstallArguments_s *ia = &rpmIArgs;
    poptContext optCon;
    int ret = 0;
    char *p[2];

    p[0] = "";
    p[1] = path;//data; // replace [abc/efg/xxx.rpm] with [abc/efg/*.rpm]
    optCon = rpmcliInit(2, p, optionsTable);

    ia->installInterfaceFlags = INSTALL_UPGRADE | INSTALL_INSTALL | INSTALL_PERCENT;

    ts = rpmtsCreate();
    rpmtsSetRootDir(ts, rpmcliRootDir);

    if (!ia->incldocs) {
        if (rpmExpandNumeric("%{_excludedocs}"))
            ia->transFlags |= RPMTRANS_FLAG_NODOCS;
    }

    // pkg is already exist, rpm --force
    ia->probFilter |= RPMPROB_FILTER_REPLACEPKG;
    //ia->probFilter |= RPMPROB_FILTER_REPLACEOLDFILES;
    ia->probFilter |= RPMPROB_FILTER_OLDPACKAGE;
    write_log(LOG_NOTICE,"rpm probFilter:use RPMPROB_FILTER_REPLACEPKG!");

    if (ia->noDeps)
        ia->installInterfaceFlags |= INSTALL_NODEPS;

    if (ia->prefix) {
        ia->relocations = malloc(2 * sizeof(*ia->relocations));
        ia->relocations[0].oldPath = NULL;   /* special case magic */
        ia->relocations[0].newPath = ia->prefix;
        ia->relocations[1].oldPath = NULL;
        ia->relocations[1].newPath = NULL;
    } else if (ia->relocations) {
        ia->relocations = realloc(ia->relocations,
            sizeof(*ia->relocations) * (ia->numRelocations + 1));
        ia->relocations[ia->numRelocations].oldPath = NULL;
        ia->relocations[ia->numRelocations].newPath = NULL;
    }
#if 1
    char* str = getenv("LANG");
    if (str == NULL) {
        str = "zh_CN.UTF-8";
    }
    write_log(LOG_NOTICE,"rpm apt:getenv lang[%s]",str);
    setenv("LC_ALL",str,1);
    str = NULL;
    str = getenv("LC_ALL");
    write_log(LOG_NOTICE,"rpm apt:getenv lc_all[%s]",str);
#endif
    ret= rpmInstall(ts, ia, (ARGV_t) poptGetArgs(optCon));
    //g_print("%s %d:file[%s] ret[%d]\n",__FUNCTION__,__LINE__,data,ret);
    if (ret != 0) {
        double progress = 110.0;
        g_progress_handle(&progress, m_arg_data,NULL);
    }
    write_debug_log("%s, line %d: install:%s %s\n", __func__, __LINE__, path,ret==0?"ok":"failed");

    rpmtsFree(ts);
    ts = NULL;

    rpmcliFini(optCon);
}

void 
install_rpm(GList *rpm_list, 
            void (*report_progress)(double *progress, void *arg_data,const char *filename),
            void *arg_data)
{
    if (!rpm_list || !report_progress)
        return;

    m_arg_data = arg_data;
    g_progress_handle = report_progress;

    g_list_foreach(rpm_list,(GFunc)do_install, NULL);
}
