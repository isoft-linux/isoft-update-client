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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>

#include <QObject>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QTreeWidget>
#include <QAction>
#include <QMenu>
#include <QDebug>
#include <QContextMenuEvent>
class KCMISoftUpdateViewer : public KCModule
{
	Q_OBJECT

public:
        explicit KCMISoftUpdateViewer(QWidget *parent, const QVariantList & list = QVariantList());
        ~KCMISoftUpdateViewer();

private:
    QTreeWidget m_viewHistory;
    void insertItem(QStringList list);
    QAction *m_cleanAll;
    void contextMenuEvent(QContextMenuEvent *event);

private slots:
    void cleanAll();


};

#endif // __MAIN_H__
