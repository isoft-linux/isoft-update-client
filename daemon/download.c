/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "download.h"

typedef struct {
    char *memory;
    size_t size;
} chunk_t;

static unsigned int resume_from = 0;

static size_t 
header_callback(char *buffer, size_t size, size_t nmemb, void *userdata)
{
	size_t realsize = size * nmemb;
    chunk_t *mem = (chunk_t *)userdata;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL)
        return 0;

    memcpy(&(mem->memory[mem->size]), buffer, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static size_t
write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    return fwrite(ptr, size, nmemb, stream);
}

static int
progress_func(void *arg, 
              curl_off_t dltotal, curl_off_t dlnow,
              curl_off_t ultotal, curl_off_t ulnow) 
{
    DownloadData *data = (DownloadData *)arg;
    curl_off_t total;
    curl_off_t point;

    total = dltotal + ultotal + resume_from;
    point = dlnow + ulnow + resume_from;

    if (data->handler_progress) {
        data->handler_progress(data->daemon, 
                               (double)point / (double)total, 
                               data->file_path);
    }
    return 0;
}

DownloadData *
download_data_new() 
{
    DownloadData *data = NULL;

    data = malloc(sizeof(DownloadData));
    if (data == NULL)
        return data;

    g_mutex_init(&data->lock);

    data->daemon                = NULL;
    data->file_path             = NULL;
    data->url                   = NULL;
    data->checksum              = NULL;
    data->handler               = NULL;
    data->handler_with_checksum = NULL;
    data->handler_error         = NULL;
    data->handler_progress      = NULL;

    return data;
}

#ifdef DLQUIT
#include <sys/syscall.h>
static DownloadData *g_data = NULL;
static CURL *g_curl = NULL;
static void dl_quit_handle(int sig)
{
    printf("in handle sig %d,[%lu]pid[%d]g_curl[%p]tid[%ld] will exit! \n", sig,pthread_self(),getpid(),g_curl,(long int)syscall(__NR_gettid));
    if (!g_data) {
        pthread_exit(NULL);
        return;
    }

    DownloadData *data = g_data;
    //bool locked = g_mutex_trylock(&data->lock);
    if (g_mutex_trylock(&data->lock)) {


        g_mutex_unlock(&data->lock);

        if (g_curl) {
            curl_easy_cleanup(g_curl);
            g_curl = NULL;
        }
        update_update_emit_error(UPDATE_UPDATE(data->daemon),
                        ERROR_FAILED,
                        g_strdup_printf("Download terminate!Failed to download files."),ERROR_CODE_OTHERS);

        gchar *updates_xml_path = NULL;
#ifndef UPDATES_XML_FILENAME
#define UPDATES_XML_FILENAME "updates.xml"
#endif
        updates_xml_path = g_strdup_printf("%s/%s",
                                           DOWNLOAD_DIR,
                                           UPDATES_XML_FILENAME);
        update_update_emit_check_update_finished(UPDATE_UPDATE(data->daemon),
                                                         updates_xml_path,
                                                         0);

        if (data) {
            if (data->daemon) {
                g_object_unref(data->daemon);
                data->daemon = NULL;
            }

            if (data->file_path) {
                free(data->file_path);
                data->file_path = NULL;
            }

            if (data->url) {
                free(data->url);
                data->url = NULL;
            }

            free(data);
            data = NULL;
        }

    }
    pthread_exit(NULL);

}
#endif
void *download_routine(void *arg) 
{
    DownloadData *data = (DownloadData *)arg;
    chunk_t chunk;
    int httperror = 0;
    struct stat fileinfo;
    char tmpfile_path[PATH_MAX] = {'\0'};
    FILE *outfile = NULL;
    CURL *curl = NULL;
    CURLcode res;

#ifdef DLQUIT
    g_data = (DownloadData *)arg;

    signal(SIGQUIT,dl_quit_handle);
    printf("\n##############in download_routine[%lu]g_data[%p]pid[%d]tid[%ld]############\n",
           pthread_self(),g_data,getpid(),(long int)syscall(__NR_gettid));
#endif
    if (!data || !data->file_path || !data->url)
        goto cleanup;

    g_mutex_lock(&data->lock);

    curl = curl_easy_init();
    if (curl == NULL) {
        printf("ERROR: failed to init curl object\n");
        goto cleanup;
    }

    chunk.memory = malloc(1);
    chunk.size = 0;

    snprintf(tmpfile_path, sizeof(tmpfile_path) - 1, "%s.st", data->file_path);
    if (stat(tmpfile_path, &fileinfo) == 0)
        resume_from = fileinfo.st_size;

    outfile = fopen(tmpfile_path, resume_from ? "ab" : "wb");
    if (outfile == NULL) {
        printf("ERROR: failed to open %s file: %s\n", 
               tmpfile_path, strerror(errno));
        goto cleanup;
    }

#ifdef DLQUIT
    g_curl = curl;
#endif

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, data->url);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    if (resume_from)
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resume_from);
    
    if (data->handler_progress) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_func);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, data);
    }

cleanup:
    g_mutex_unlock(&data->lock);
#if DEBUG
    printf("DEBUG: %s, line %d: Try downloading from URL %s\n", 
            __func__, __LINE__, data->url);
#endif
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (data->handler_error) {
            data->handler_error(data->daemon, data->file_path, data->url, res);
        }

       update_update_emit_error(UPDATE_UPDATE(data->daemon),
                                 ERROR_CHECK_UPDATE,
                                 g_strdup_printf("Network error"),
                                ERROR_CODE_NETWORK);
    }

    if (chunk.memory) {
        if (strstr(chunk.memory, "404 Not Found")) {
            httperror = 1;
            printf("ERROR: %s, %s, line %d: %s\n", __FILE__, __func__, __LINE__, chunk.memory);
            if (data->handler_error) {
                data->handler_error(data->daemon, data->file_path, data->url, -1);
            }
        }
        free(chunk.memory);
        chunk.memory = NULL;
    }

    write_debug_log("%s, line %d: downloaded [%s],ret[%d]\n",
            __func__, __LINE__, data->url,res);

    if (outfile) {
        fflush(outfile);
        fclose(outfile);
        outfile = NULL;
    }

    if (rename(tmpfile_path, data->file_path) == -1) {
        printf("ERROR: failed to rename %s to %s", 
               tmpfile_path, data->file_path);
    }

#if DEBUG
    printf("DEBUG: %s, line %d: Save to %s\n",
           __func__, __LINE__, data->file_path);
#endif

    if (curl) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }

    g_mutex_lock(&data->lock);

    if (data->handler && !httperror)
        data->handler(data->daemon, data->file_path, data->url);

    if (data->handler_with_checksum && !httperror) {
        data->handler_with_checksum(data->daemon, 
                                    data->file_path, 
                                    data->url, 
                                    data->checksum);

    }

    g_mutex_unlock(&data->lock);

    if (data) {
        if (data->daemon) {
            g_object_unref(data->daemon);
            data->daemon = NULL;
        }

        if (data->file_path) {
            free(data->file_path);
            data->file_path = NULL;
        }

        if (data->url) {
            free(data->url);
            data->url = NULL;
        }

        free(data);
        data = NULL;
    }

    return NULL;
}
