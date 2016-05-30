/*
 * Copyright 2015  Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __ISOFTUPDATE_H__
#define __ISOFTUPDATE_H__

#include <QObject>

#include "updategenerated.h"

class UpdateObject;

class ISoftUpdate : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool updateEnable READ updateEnable)
    Q_PROPERTY(QList<QObject*> updates READ updates)

public:
    explicit ISoftUpdate(QObject *parent = Q_NULLPTR);
    virtual ~ISoftUpdate();

    bool updateEnable() const;
    QList<QObject*> updates() const;

    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE void downloadUpdate(const QString &file);
    Q_INVOKABLE void installUpdate();
    Q_INVOKABLE void reboot();

Q_SIGNALS:
    void errored(qlonglong error, const QString &details,qlonglong errcode);
    void updateEnableChanged(bool enable);
    void checkUpdateFinished(const QString &file);
    void percentChanged(qlonglong status, 
                        const QString &file, 
                        double percent, 
                        QString percentStr);
    void finished(qlonglong status);
    void needReboot(bool need);
    void typeChanged(qlonglong status);

private:
    org::isoftlinux::Update *m_update = Q_NULLPTR;
    QList<QObject*> m_updates;
};

class UpdateObject : public QObject 
{
    Q_OBJECT

    Q_PROPERTY(QString id READ id)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString summary READ summary)

public:
    explicit UpdateObject(const QString &id, 
                          const QString &type, 
                          const QString &summary, 
                          QObject *parent = 0) 
      : QObject(parent),
        m_id(id),
        m_type(type),
        m_summary(summary)
    {
    }

    QString id() const { return m_id; }
    QString type() const { return m_type; }
    QString summary() const { return m_summary; }

private:
    QString m_id;
    QString m_type;
    QString m_summary;
};

#endif // __ISOFTUPDATE_H__
