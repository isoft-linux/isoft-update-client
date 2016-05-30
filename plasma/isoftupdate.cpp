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

#include "isoftupdate.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDomDocument>

#include <KToolInvocation>
#include <KLocalizedString>

#include <kworkspace.h>

static const QString latestStr = "latest";

ISoftUpdate::ISoftUpdate(QObject *parent):
    QObject(parent)
{
    m_update = new org::isoftlinux::Update("org.isoftlinux.Update", 
                                           "/org/isoftlinux/Update", 
                                           QDBusConnection::systemBus(), 
                                           this);

    connect(m_update, &org::isoftlinux::Update::Error, 
            [this](qlonglong error, const QString &details,qlonglong errcode) {
                Q_EMIT errored(error, details,errcode);
            });

    connect(m_update, &org::isoftlinux::Update::UpdateEnableChanged, 
            [this](bool enable) {
                Q_EMIT updateEnableChanged(enable);
            });

    connect(m_update, &org::isoftlinux::Update::CheckUpdateFinished,
            [this](const QString &file, qlonglong latest) {
                Q_FOREACH (QObject *obj, m_updates) {
                    if (obj) {
                        delete obj;
                        obj = Q_NULLPTR;
                    }
                }
                m_updates.clear();

                QFile xmlFile(file);
                if (xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QDomDocument doc;
                    if (doc.setContent(QString(xmlFile.readAll()))) {
                        QDomElement root = doc.documentElement();
                        if (root.hasAttribute(latestStr)) {
                            if (latest < root.attribute(latestStr).toLongLong()) {
                                QDomElement update = root.firstChildElement("update");
                                while (!update.isNull()) {
                                    QDomElement id = update.firstChildElement("id");
                                    QDomElement type = update.firstChildElement("type");
                                    QDomElement summary = update.firstChildElement("summary");
                                    if (!id.isNull()) {
                                        if (latest < id.text().toLongLong() &&
                                            !type.isNull() && 
                                            !summary.isNull()) {
                                            QString sum(summary.text());

                                            char tmpStr[128]="";
                                            snprintf(tmpStr,sizeof(tmpStr),"%s",qPrintable(sum));
                                            bool hasHZ = false;
                                            for(int i = 0 ; i < strlen(tmpStr) ; i++) {
                                                unsigned char c = tmpStr[i];
                                                if(c >= 0x80) {
                                                    hasHZ = true;
                                                    break;
                                                }
                                            }
                                            // 20 characters for hz;40 for en
                                            if (summary.text().size() > 20) {
                                                if(hasHZ)
                                                    sum = sum.leftJustified(20,'.',true);
                                                else
                                                    sum = sum.leftJustified(40,'.',true);

                                                sum.append("...");
                                            } else {
                                                if(hasHZ)
                                                    sum = sum.leftJustified(20,'.',true);
                                                else
                                                    sum = sum.leftJustified(40,'.',true);

                                                sum.append("...");
                                            }
                                            QString tp(type.text());
                                            QString tp2(i18n(qPrintable(tp)));
                                            sum.append(" ["+ tp2  + "]");

                                            m_updates.append(new UpdateObject(id.text(), 
                                                type.text(), sum));

                                        }
                                    }

                                    update = update.nextSiblingElement("update");
                                }
                            }
                        }
                    }

                    xmlFile.close();
                }
                if (xmlFile.exists())
                    Q_EMIT checkUpdateFinished(file);
            });

    connect(m_update, &org::isoftlinux::Update::PercentChanged,
            [this](qlonglong status, const QString &file, double percent) {
                Q_EMIT percentChanged(status, file, percent, 
                        QString("%1%").arg(percent * 100.0, 0, 'f', 0));
            });

    connect(m_update, &org::isoftlinux::Update::Finished, 
            [this](qlonglong status) {
                Q_EMIT finished(status);
            });

    connect(m_update, &org::isoftlinux::Update::TypeChanged,
            [this](qlonglong status) {
                Q_EMIT typeChanged(status);
            });

    connect(m_update, &org::isoftlinux::Update::NeedReboot, 
            [this](bool need) {
                Q_EMIT needReboot(need);
            });
}

ISoftUpdate::~ISoftUpdate()
{
}

bool ISoftUpdate::updateEnable() const 
{
    return m_update->updateEnable();
}

void ISoftUpdate::checkUpdate() 
{
    m_update->CheckUpdate();
}

void ISoftUpdate::downloadUpdate(const QString &file) 
{
    m_update->DownloadUpdate(file);
}

void ISoftUpdate::installUpdate() 
{
    m_update->InstallUpdate();
}

void ISoftUpdate::reboot() 
{
    KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmDefault, 
                                KWorkSpace::ShutdownTypeReboot);
}

QList<QObject*> ISoftUpdate::updates() const { return m_updates; }

#include "isoftupdate.moc"
