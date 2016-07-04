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

#include "updategenerated.h"

// #define USEAUTOINSTALL

K_PLUGIN_FACTORY(Factory,
        registerPlugin<KCMISoftUpdate>();
        )
K_EXPORT_PLUGIN(Factory("isoftupdate"))


org::isoftlinux::Update *m_update = Q_NULLPTR;

KCMISoftUpdate::KCMISoftUpdate(QWidget *parent, const QVariantList &)
  : KCModule(parent)
{
    setButtons(Help | Apply);

    QHBoxLayout *topLayout = new QHBoxLayout(this);
    topLayout->setSpacing(KDialog::spacingHint());
    topLayout->setMargin(0);

    topLayout->addSpacing(22);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(10);
    topLayout->addLayout(formLayout);


    QLabel *subTitle = new QLabel(i18n("Configuration"));
    //subTitle->setStyleSheet("QLabel { color: #cccccc; }");
    formLayout->addRow(subTitle);

    _enableUpdate = new QCheckBox;
    formLayout->addRow(i18n("Enable Update:"), _enableUpdate);
    QLabel *infoLabel = new QLabel(i18n("It will automatically check update if enable update.\n"));
    infoLabel->setStyleSheet("QLabel { color: #cccccc; }");
    formLayout->setVerticalSpacing(formLayout->verticalSpacing()-15);
    formLayout->addRow(infoLabel);

    _autoDlBox = new QCheckBox;
    formLayout->addRow(i18n("Automatic Download:"), _autoDlBox);
    QLabel *infoLabel2 = new QLabel(i18n("If this check box is checked, it will automatically download packages.\n"));
    infoLabel2->setStyleSheet("QLabel { color: #cccccc; }");
    formLayout->addRow(infoLabel2);

    _autoInstallBox = new QCheckBox;
    formLayout->addRow(i18n("Automatic Install:"), _autoInstallBox);
    QLabel *infoLabel3 = new QLabel(i18n("If this check box is checked, it will automatically install packages.\n"));
    infoLabel3->setStyleSheet("QLabel { color: #cccccc; }");
    formLayout->addRow( infoLabel3);

    QHBoxLayout *horizontalLayout;
    horizontalLayout = new QHBoxLayout();
            horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
            horizontalLayout->setContentsMargins(0, 0, 0, 0);

    _setUptDuration = new QComboBox;
    _setUptDuration->insertItems(0, QStringList()
             << i18n("20 minutes")
             << i18n("1 hour")
             << i18n("1 day")
            );

    horizontalLayout->addWidget(_setUptDuration);
    horizontalLayout->setSpacing(10);
    QLabel *lisLabel = new QLabel(i18n("Listening NetworkManager Signal:StateChanged."));
    lisLabel->setStyleSheet("QLabel { color: #cccccc; }");
    horizontalLayout->addWidget(lisLabel);

    formLayout->addRow(i18n("Set Update Duration:"), horizontalLayout);

    QLabel *uptRecord = new QLabel(i18n("<a href=\"#\">Update Record</a>"));

    connect(uptRecord, &QLabel::linkActivated, [this]() {
                KToolInvocation::kdeinitExec(QString("kcmshell5"),
                    QStringList() << QString("kcm_isoftupdate_viewer"));
            });

    formLayout->addRow(uptRecord);

    QLabel *versionLabel = new QLabel(i18n("<a href=\"#\">Version</a>"));
    connect(versionLabel, &QLabel::linkActivated, [this]() {
                KToolInvocation::kdeinitExec(QString("kcmshell5"), 
                    QStringList() << QString("about-distro"));
            });
    formLayout->addRow(versionLabel);

    m_update = new org::isoftlinux::Update("org.isoftlinux.Update",
                                           "/org/isoftlinux/Update",
                                           QDBusConnection::systemBus(),
                                           this);

    if (m_update == NULL) {
            return ;
    }

    load();

    connect(_enableUpdate, SIGNAL(clicked()), this, SLOT(enableChanged()));
    connect(_autoDlBox, SIGNAL(clicked()), this, SLOT(autoDlChanged()));
#ifdef USEAUTOINSTALL
    connect(_autoInstallBox, SIGNAL(clicked()), this, SLOT(changed()));
#endif
    connect(_setUptDuration, SIGNAL(currentIndexChanged(int)), this, SLOT(uptDurationChanged(int)));

}

KCMISoftUpdate::~KCMISoftUpdate()
{
}

void KCMISoftUpdate::autoDlChanged()
{
    bool autoDl = _autoDlBox->isChecked();
#ifdef USEAUTOINSTALL
    bool autoINstall = _autoInstallBox->isChecked();
    if (!autoDl) {
        _autoInstallBox->setEnabled(autoDl);
    }
    else {
        _autoInstallBox->setEnabled(true);
    }
#endif

    changed();

}


void KCMISoftUpdate::enableChanged()
{
    bool enable = _enableUpdate->isChecked();
    bool autoDL = _autoDlBox->isChecked();

    _autoDlBox->setEnabled(enable);
#ifdef USEAUTOINSTALL
    _autoInstallBox->setEnabled(enable);
#endif
    _setUptDuration->setEnabled(enable);
#ifdef USEAUTOINSTALL
    if (!autoDL) {
        _autoInstallBox->setEnabled(autoDL);
    }
#endif
    changed();

}


void KCMISoftUpdate::uptDurationChanged(int index)
{
    _setUptDuration->setCurrentIndex(index);
    changed();
}

void KCMISoftUpdate::load()
{
    bool enable = m_update->updateEnable();
    qulonglong updateDuration = m_update->updateDuration();
    bool autoInstall = m_update->autoInstall();
    bool autoDownload = m_update->autoDownload();

    _enableUpdate->setChecked(enable);
    _autoDlBox->setChecked(autoDownload);
#ifdef USEAUTOINSTALL
    _autoInstallBox->setChecked(autoInstall);
#endif
    int index = getDurIndex(updateDuration);
    _setUptDuration->setCurrentIndex(index);

    _autoDlBox->setEnabled(enable);
#ifdef USEAUTOINSTALL
    _autoInstallBox->setEnabled(enable);
#endif
    _setUptDuration->setEnabled(enable);
#ifdef USEAUTOINSTALL
    if (!autoDownload) {
        _autoInstallBox->setEnabled(autoDownload);
    }
#else
    _autoInstallBox->setEnabled(false);
#endif
}

int KCMISoftUpdate::getDurIndex(qulonglong Duration)
{
    int index = -1;
    qulonglong sec = Duration/1000ULL;
    if (sec < (qulonglong)60*60) {
        index = 0;
    }
    else if (sec == (qulonglong)60*60) {
        index = 1;
    }
    else if (sec == (qulonglong)24*60*60) {
        index = 2;
    }
    else if (sec == (qulonglong)7*24*60*60) {
        index = 3;
    }
    else {
        index = 4;
    }
    return index;
}

qulonglong KCMISoftUpdate::getDuration(int index)
{
    qulonglong ret = 0ULL;

    if (index == 0) {
        ret = (qulonglong)20*60;
    }
    else if (index == 1) {
        ret = (qulonglong)60*60;
    }
    else if (index == 2) {
        ret = (qulonglong)24*60*60;
    }
    else if (index == 3) {
        ret = (qulonglong)7*24*60*60;
    }
    else {
        ret = (qulonglong)30*24*60*60;
    }

    return ret*1000ULL;
}

void KCMISoftUpdate::save() 
{
    bool changed = false;
    bool enable = m_update->updateEnable();
    qulonglong updateDuration = m_update->updateDuration();
    bool autoInstall = m_update->autoInstall();
    bool autoDownload = m_update->autoDownload();

    if (enable != _enableUpdate->isChecked()) {
        m_update->SetUpdateEnable(_enableUpdate->isChecked());
        changed = true;
    }
    if (autoDownload != _autoDlBox->isChecked()) {
        m_update->SetAutoDownload(_autoDlBox->isChecked());
        changed = true;
    }
#ifdef USEAUTOINSTALL
    if (autoInstall != _autoInstallBox->isChecked()) {
        m_update->SetAutoInstall(_autoInstallBox->isChecked());
        changed = true;
    }
#endif
    int index = getDurIndex(updateDuration);
    if( index != _setUptDuration->currentIndex()) {
        index = _setUptDuration->currentIndex();
        updateDuration = getDuration(index);
        m_update->SetUpdateDuration(updateDuration);
        changed = true;
    }

    if (changed) {
        m_update->SaveConfFile();
    }

}

#include "main.moc"
