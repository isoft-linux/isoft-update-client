/**
 *  Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#include "updategenerated.h"

class KCMISoftUpdate : public KCModule
{
	Q_OBJECT

public:
	explicit KCMISoftUpdate(QWidget *parent, const QVariantList & list = QVariantList());
        ~KCMISoftUpdate();

	void load();
    void save();

private:
    QCheckBox *_enableUpdate = nullptr;
    QCheckBox *_autoDlBox = nullptr;
    QCheckBox *_autoInstallBox = nullptr;
    QComboBox *_setUptDuration = nullptr;

    int getDurIndex(qulonglong Duration);
    qulonglong getDuration(int index);

private slots:
    void uptDurationChanged(int index);
    void enableChanged();
    void autoDlChanged();
};

#endif // __MAIN_H__
