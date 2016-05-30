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

#ifndef __DAEMON_H__
#define __DAEMON_H__

#include "types.h"
#include "update-generated.h"
#include "utils.h"

G_BEGIN_DECLS

#define TYPE_DAEMON         (daemon_get_type())
#define DAEMON(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), TYPE_DAEMON, Daemon))
#define DAEMON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_DAEMON, DaemonClass))
#define IS_DAEMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), TYPE_DAEMON))
#define IS_DAEMON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), TYPE_DAEMON))
#define DAEMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), TYPE_DAEMON, DaemonClass))

typedef struct DaemonClass DaemonClass;
typedef struct DaemonPrivate DaemonPrivate;

struct Daemon {
    UpdateUpdateSkeleton parent;
    DaemonPrivate *priv;
};

struct DaemonClass {
    UpdateUpdateSkeletonClass parent_class;
};

typedef enum {
    STATUS_CHECK_UPDATE,
    STATUS_DOWNLOAD_UPDATE,
    STATUS_INSTALL_UPDATE
} Status;

typedef enum {
    TYPE_NORMAL,
    TYPE_SECURITY
} uptType;

typedef enum {
    ERROR_FAILED,
    ERROR_PERMISSION_DENIED,
    ERROR_NOT_SUPPORTED,
    ERROR_TASK_LOCKED,
    ERROR_CHECK_UPDATE,
    ERROR_INSTALL_UPDATE,
    NUM_ERRORS
} Error;

typedef enum {
    ERROR_CODE_NETWORK,
    ERROR_CODE_DOWNLOAD,
    ERROR_CODE_INSTALL,
    ERROR_CODE_OTHERS
} ErrorCode;

#define ERROR error_quark()

GType error_get_type();
#define TYPE_ERROR (error_get_type())
GQuark error_quark();

GType   daemon_get_type              (void) G_GNUC_CONST;
Daemon *daemon_new                   (void);

/* local methods */

GHashTable * daemon_read_extension_ifaces();
GHashTable * daemon_get_extension_ifaces(Daemon *daemon);

G_END_DECLS

#endif /* __DAEMON_H__ */
