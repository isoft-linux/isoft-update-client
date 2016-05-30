/**
 *  Copyright (C) 2015 fjiang <fujiang.zhu@i-soft.com.cn>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 */

#include "main.h"

#include <kdialog.h>
#include <kaboutdata.h>
#include <KToolInvocation>

#include <KPluginFactory>
#include <KPluginLoader>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QIcon>
#include <QMessageBox>
#include <KLocalizedString>
#include <QFile>
#include <QTextStream>
#include "updategenerated.h"

K_PLUGIN_FACTORY(Factory,
        registerPlugin<KCMISoftUpdateViewer>();
        )
K_EXPORT_PLUGIN(Factory("isoftupdate"))

org::isoftlinux::Update *m_update = Q_NULLPTR;


#ifndef ISOFT_UPDATED_FILE_PATH
#define ISOFT_UPDATED_FILE_PATH "/etc/update/updates/updated.log"
#endif

#ifndef ISOFT_UPDATING_FILE_PATH
#define ISOFT_UPDATING_FILE_PATH "/etc/update/updates/updating.log"
#endif

KCMISoftUpdateViewer::KCMISoftUpdateViewer(QWidget *parent, const QVariantList &)
  : KCModule(parent)
{
    QVBoxLayout *mainLayout=new QVBoxLayout(this);
    mainLayout->setMargin(KDialog::marginHint());
    mainLayout->setSpacing(KDialog::spacingHint());

    mainLayout->addWidget(&m_viewHistory, 1);

    m_viewHistory.setAllColumnsShowFocus(true);
    m_viewHistory.setFocusPolicy(Qt::ClickFocus);

    QStringList headers;
    headers << i18n("Name") << i18n("Status") << i18n("Type") << i18n("Date") << i18n("Summary");
    m_viewHistory.setHeaderLabels(headers);

    m_viewHistory.setWhatsThis(i18n("<p>If the list is empty, try checking "
                                    "the log file is exists or not.</p>") );

    m_viewHistory.setMinimumSize(400, 300);
    m_viewHistory.setSortingEnabled(true);
    m_cleanAll = new QAction(i18n("Clean all"),this);
    connect(m_cleanAll, SIGNAL(triggered()), this, SLOT(cleanAll()));

    QFile file(ISOFT_UPDATING_FILE_PATH);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        while (!line.isNull()) {
            QStringList list = line.split("|||");
            insertItem(list);
            line = in.readLine();
        };
    }

    QFile fileUpdated(ISOFT_UPDATED_FILE_PATH);
    if (fileUpdated.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream inUpdated(&fileUpdated);
        QString lineUpdated = inUpdated.readLine();
        while (!lineUpdated.isNull()) {
            QStringList list = lineUpdated.split("|||");
            insertItem(list);
            lineUpdated = inUpdated.readLine();
        }
    }

    m_update = new org::isoftlinux::Update("org.isoftlinux.Update",
                                           "/org/isoftlinux/Update",
                                           QDBusConnection::systemBus(),
                                           this);

    if (m_update == NULL) {
            return ;
    }

}

KCMISoftUpdateViewer::~KCMISoftUpdateViewer()
{
}

void KCMISoftUpdateViewer::insertItem(QStringList list)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(&m_viewHistory);
    char buffer[512]="";
    QStringList::const_iterator constIterator;
    int i = 0;
    for (constIterator = list.constBegin(); constIterator != list.constEnd();
               ++constIterator,i++) {
        if (i == 0) {
            item->setText(0, (*constIterator).toLocal8Bit().constData());
        } else if (i == 1) {
            snprintf(buffer,sizeof(buffer),(*constIterator).toLocal8Bit().constData());

            if (atoi(buffer) == 1) {
                item->setText(1, i18n("Succeed"));
            } else if (atoi(buffer) == 0){
                item->setText(1, i18n("Failed"));
            } else {
                item->setText(1, i18n("Uninstalled"));
                item->setBackground(0,QBrush(QColor("#339966")));
                item->setBackground(1,QBrush(QColor("#339966")));
                item->setBackground(2,QBrush(QColor("#339966")));
                item->setBackground(3,QBrush(QColor("#339966")));
                item->setBackground(4,QBrush(QColor("#339966")));
            }
        } else if (i == 2) {
            snprintf(buffer,sizeof(buffer),list.at(2).toLocal8Bit().constData());
            if (strcmp(buffer,"bugfix") == 0) {
                item->setText(2, i18n("bugfix"));
            } else if (strcmp(buffer,"update") == 0) {
                item->setText(2, i18n("update"));
            } else if (strcmp(buffer,"security") == 0) {
                item->setText(2, i18n("security"));
            } else {
                item->setText(2, i18n("unknown"));
            }
        } else if (i == 3) {
            snprintf(buffer,sizeof(buffer),(*constIterator).toLocal8Bit().constData());
            item->setText(3, buffer);
        } else if (i == 4) {
            snprintf(buffer,sizeof(buffer),(*constIterator).toLocal8Bit().constData()); // summary
            item->setText(4, buffer);
        } else if (i == 5) {
            snprintf(buffer,sizeof(buffer),(*constIterator).toLocal8Bit().constData()); // desc in tooltip

            QString line(buffer);
            memset(buffer,0,sizeof(buffer));
            int len = 0;
            if (!line.isNull()) {
                QStringList lst = line.split("///");

                QStringList::const_iterator it;
                int j = 0;
                for (it = lst.constBegin(); it != lst.constEnd();++it) {
                    if(strlen((*it).toLocal8Bit().constData()) <2)
                        continue;

                    if (j <= 10) {
                        len = 500 - strlen(buffer);
                        if (len > strlen(buffer)) {
                            if(strlen(buffer)>1)
                                strcat(buffer,"\n");
                            strcat(buffer,(*it).toLocal8Bit().constData());
                        } else
                            break;
                    } else
                        break;
                    j++;
                }

            }
            for (int j =0; j<5;j++) {
                item->setToolTip(j,buffer);
            }
        }
    }
}

void KCMISoftUpdateViewer::contextMenuEvent(QContextMenuEvent *event)
{
    QTreeWidgetItem *item = m_viewHistory.currentItem();

    if(0&&item != NULL)
    {
       QMenu *popMenu =new QMenu(this);

       popMenu->addAction(m_cleanAll);

       popMenu->exec(QCursor::pos());
    }

    event->accept();
}

void KCMISoftUpdateViewer::cleanAll()
{
    qDebug() << "\n will clean all records\n";
    if (m_update) {
        m_viewHistory.clear();
        m_update->CleanUptRecord();
    }

}

#include "main.moc"
