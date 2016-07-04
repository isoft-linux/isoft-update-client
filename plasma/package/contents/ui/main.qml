/*
    Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
    Copyright (C) 2015 fujiang <fujiang.zhu@i-soft.com.cn>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

import QtQuick 2.0
import QtQuick.Controls 1.1
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.kquickcontrolsaddons 2.0
import org.kde.plasma.isoftupdate 1.0

Item {
    id: root;
    width: 290; height: 190

    property bool showPopup: false
    property int updateAction: 0
    property string updateFile: ""

    ISoftUpdate {
        id: isoftUpdate

        Component.onCompleted: {
            plasmoid.icon = isoftUpdate.updateEnable ?
                        "security-high" :
                        "security-low";
            //plasmoid.status = isoftUpdate.updateEnable ?
            //            PlasmaCore.Types.HiddenStatus :
            //            PlasmaCore.Types.ActiveStatus;
            plasmoid.status = PlasmaCore.Types.ActiveStatus;
            if (!isoftUpdate.updateEnable) {
                infoText.text = i18n("Update is disable");
                infoBtn.visible = false;
            }
            //plasmoid.expanded = false;
        }

        onErrored: {
            if (isoftUpdate.updateEnable) {
                //plasmoid.expanded = false;
                if (errcode == 1) {
                    infoText.text = i18n("Download error");
                } else if (errcode == 2) {
                    infoText.text = i18n("Install error");
                }
            }
        }

        onUpdateEnableChanged: {
            //plasmoid.status = enable ?
            //            PlasmaCore.Types.HiddenStatus :
            //            PlasmaCore.Types.ActiveStatus;
            plasmoid.icon = enable ? "security-high" : "security-low";
            percentItem.visible = false;
            infoItem.visible = true;
            updateListView.visible = false;
            if (enable) {
                infoBtn.visible = true;
                infoBtn.anchors.top = infoText.bottom;
                infoText.text = i18n("There is nothing to do today");
            } else {
                infoBtn.visible = false;
                infoText.text = i18n("Update is disable");
            }
        }

        onPercentChanged: {
            if (root.showPopup == false) {
                //plasmoid.expanded = false;
                root.showPopup = true;
                plasmoid.status = PlasmaCore.Types.ActiveStatus;
            }

            if(plasmoid.status != PlasmaCore.Types.ActiveStatus) {
                plasmoid.status = PlasmaCore.Types.ActiveStatus;
            }

            if (infoItem.visible) {
                infoItem.visible = false;
            }
            if (!percentItem.visible) {
                percentItem.visible = true;
            }

            if (status == 1) {
                percentText.text = i18n("Downloading %1", percentStr);
            } else if (status == 2) {
                percentText.text = i18n("Installing %1", percentStr);
            }
            progressBar.value = percent;
        }

        onCheckUpdateFinished: {
            root.updateFile = file;
            root.showPopup = false;
            //plasmoid.expanded = false;
            plasmoid.status = PlasmaCore.Types.ActiveStatus;

            infoItem.visible = true;
            percentItem.visible = false;

            root.updateAction = 1;
            infoText.visible = false;
            updateListView.model = isoftUpdate.updates;
            updateListView.visible = true;
            infoBtn.anchors.top = updateListView.bottom; // to restore infoText.bottom
            infoBtn.text = i18n("Manual Download");
        }

        onFinished: {
            root.showPopup = false;
            //plasmoid.expanded = false;
            plasmoid.status = PlasmaCore.Types.ActiveStatus;

            infoItem.visible = true;
            infoText.visible = true;
            updateListView.visible = false;
            percentItem.visible = false;

            root.updateAction = status + 1;
            if (status == 1) {
                infoText.text = i18n("Downloaded Update");
                infoBtn.anchors.top = infoText.bottom;
                infoBtn.text = i18n("Reboot to install"); //i18n("Manual Install");
            } else if (status == 2) {
                infoText.text = i18n("Installed Update");
                infoBtn.anchors.top = infoText.bottom;
                infoBtn.text = i18n("Manual Check"); // set default
                root.updateAction = 0; // set default
            }
        }

        onNeedReboot: {
            root.updateAction = 3;
            if (need) {
                infoText.text = i18n("Installed Update");
                infoBtn.anchors.top = infoText.bottom;
                infoBtn.text = i18n("Manual Reboot");
            }
        }

        onTypeChanged: {
            plasmoid.status = PlasmaCore.Types.ActiveStatus;
            if (status == 0) {
                plasmoid.icon = "security-high";
            } else if (status == 1) {
                plasmoid.icon = "security-high-red";
            }
        }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "#dcddde"
    }

    Item {
        id: infoItem
        anchors.fill: parent

        Text {
            id: infoText
            text: i18n("There is nothing to do today")
            font.pixelSize: 18
            anchors.top: parent.top
            anchors.topMargin: 68
            anchors.horizontalCenter: parent.horizontalCenter
        }

        ListView {
            id: updateListView
            visible: false
            width: parent.width - 15
            height: 200
            anchors.top: parent.top
            section.property: modelData.type
            delegate: Item {
                width: parent.width
                height: 30

                Text {
                    text: modelData.summary
                    color: getUptType() //modelData.type == "security" ? "red" : "black"
                }
                function getUptType() {
                    if (modelData.type == "security") {
                        plasmoid.icon = "security-high-red";
                    }
                    else {
                        plasmoid.icon = "security-high"
                    }
                    return modelData.type == "security" ? "red" : "black";
                }
            }
        }

        Button {
            id: infoBtn
            text: i18n("Manual Check")
            anchors.top: infoText.bottom
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                if (root.updateAction == 0) {
                    infoText.text = i18n("There is nothing to do today"); // set default
                    isoftUpdate.checkUpdate();
                } else if (root.updateAction == 1) {
                    isoftUpdate.downloadUpdate(root.updateFile);
                    updateListView.visible = false;
                } else if (root.updateAction == 2) {
                    // reboot, then will install...
                    //isoftUpdate.installUpdate();
                    isoftUpdate.reboot();
                } else if (root.updateAction == 3) {
                    isoftUpdate.reboot();
                }
            }
        }
    }

    Item {
        id: percentItem
        visible: false
        anchors.top: parent.top
        anchors.left: parent.left

        Text {
            id: statusText
            text: i18n("System Update Progress")
            font.pixelSize: 13
            anchors.top: parent.top
            anchors.topMargin: 39
            anchors.left: parent.left
            anchors.leftMargin: 32
        }

        ProgressBar {
            id: progressBar
            width: 300
            anchors.top: statusText.bottom
            anchors.left: statusText.left
            anchors.topMargin: 22
        }

        Text {
            id: percentText
            anchors.top: progressBar.bottom
            anchors.left: statusText.left
            anchors.topMargin: 9
        }
    }

    Rectangle {
        width: parent.width
        height: 1
        color: "#dcddde"
        anchors.bottom: kcmText.top
        anchors.bottomMargin: 5
    }

    Text {
        id: kcmText
        text: i18n("System Update Settings")
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter

        property color buttonColor: "#ADD8E6"

        MouseArea {
            anchors.fill: parent

            hoverEnabled: true

            cursorShape: Qt.PointingHandCursor
            onClicked: {
                KCMShell.open("kcm_isoftupdate");
            }
        }
    }
}
