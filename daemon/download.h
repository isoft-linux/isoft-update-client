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

#ifndef __DOWNLOAD_H__
#define __DOWNLOAD_H__

#include <curl/curl.h>

#include "daemon.h"

//#define DLQUIT // test: cancel thread

/* 下载完成后的回调函数 */
typedef void (*downloaded_handler)(Daemon   *daemon, 
                                   gchar    *file_path, 
                                   gchar    *url);
/* 下载完成后，带 checksum 参数，这个用在检查 upt 文件的 md5sum */
typedef void (*downloaded_handler_with_checksum)(Daemon *daemon, 
                                                 gchar  *file_path, 
                                                 gchar  *url, 
                                                 gchar  *checksum);
/* 下载出错的回调函数 */
typedef void (*downloaded_handler_error)(Daemon     *daemon, 
                                         gchar      *file_path, 
                                         gchar      *url, 
                                         CURLcode   error);
/* 下载进度的回调函数 */
typedef void (*downloaded_handler_progress)(Daemon  *daemon,
                                            gdouble percent, 
                                            gchar   *file_path);

typedef struct {
    Daemon *daemon;

    GMutex lock;

    char *file_path;
    char *url;
    char *checksum;
    
    downloaded_handler handler;
    downloaded_handler_with_checksum handler_with_checksum;
    downloaded_handler_error handler_error;
    downloaded_handler_progress handler_progress;
} DownloadData;

/* 构造函数，将成员全部指向空指针 */
DownloadData *download_data_new();

/* 下载回调函数，线程安全 */
void *download_routine(void *arg);

#endif /* __DOWNLOAD_H__ */
