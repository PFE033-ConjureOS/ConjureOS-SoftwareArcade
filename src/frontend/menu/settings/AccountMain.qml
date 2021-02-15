// Pegasus Frontend
// Copyright (C) 2017-2018  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


import "common"
import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.0
import QtQuick.Window 2.2


FocusScope {
    id: root

    signal close
//    signal openKeySettings
//    signal openGamepadSettings
    signal openGameDirSettings
    signal openMenuBoxSettings

    width: parent.width
    height: parent.height

    visible: 0 < (x + width) && x < Window.window.width

    enabled: focus

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.close();
        }
    }


    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.close()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: root.close()
    }

    ScreenHeader {
        id: header
        text: qsTr("Account") + api.tr
        z: 2
    }

    Flickable {
        id: container

        width: content.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        contentWidth: content.width
        contentHeight: content.height

        Behavior on contentY { PropertyAnimation { duration: 100 } }

        readonly property int yBreakpoint: height * 0.7
        readonly property int maxContentY: contentHeight - height

        function onFocus(item) {
            if (item.focus)
                contentY = Math.min(Math.max(0, item.y - yBreakpoint), maxContentY);
        }

        FocusScope {
            id: content

            focus: true
            enabled: focus

            width: contentColumn.width
            height: contentColumn.height

            Column {
                id: contentColumn
                spacing: vpx(5)

                width: root.width * 0.7
                height: implicitHeight

                Item {
                    width: parent.width
                    height: header.height + vpx(25)
                }

                SectionTitle {
                    text: qsTr("Retroachievement") + api.tr
                    first: true
                }

                ToggleOption {
                    id: optRetroachievementActivate

                    focus: true
                    label: qsTr("Activate Retroachievement") + api.tr
                    note: qsTr("Unlock Trophées") + api.tr

//                    checked: api.internal.settings.fullscreen
                    onCheckedChanged: {
                        focus = true;
//                        api.internal.settings.fullscreen = checked;
                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.up: optNetplayPswd
                    KeyNavigation.down: optRetroachievementLoginIn
                }

                SimpleButton {
                    id: optRetroachievementLoginIn

                    label: qsTr("Connect Retroachievement") + api.tr

                    onActivate: {
                        focus = true;
                        root.openMenuBoxSettings();
                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.down: optNetplayActivate
                }

                SectionTitle {
                    text: qsTr("Netplay") + api.tr
                    first: true
                }

                ToggleOption {
                    id: optNetplayActivate

                    label: qsTr("Activate Netplay") + api.tr
                    note: qsTr("Play with your friends online") + api.tr

//                    checked: api.internal.settings.fullscreen
                    onCheckedChanged: {
                        focus = true;
//                        api.internal.settings.fullscreen = checked;
//                        pop menu if activate
//                        root.openGameDirSettings();

                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.down: optNetplayNickname
                }

                MultivalueOption {
                    id: optNetplayNickname

                    label: qsTr("Netplay Nickname") + api.tr
//                    value: api.internal.settings.locales.currentName

                    onActivate: {
                        focus = true;
                        root.openMenuBoxSettings();
                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.down: optNetplayPswdActivate
                }

                SectionTitle {
                    text: qsTr("Password Netplay") + api.tr
                    first: true
                }

                ToggleOption {
                    id: optNetplayPswdActivate

                    label: qsTr("Activate Password Netplay") + api.tr
                    note: qsTr("Set password on your game room") + api.tr

//                    checked: api.internal.settings.fullscreen
                    onCheckedChanged: {
                        focus = true;
//                        api.internal.settings.fullscreen = checked;
//                        root.openGameDirSettings();
                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.down: optNetplayPswd
                }

                SimpleButton {
                    id: optNetplayPswd

                    label: qsTr("Netplay Password") + api.tr
//                    value: api.internal.settings.locales.currentName

                    onActivate: {
                        focus = true;
                        root.openMenuBoxSettings();
                    }
                    onFocusChanged: container.onFocus(this)

                    KeyNavigation.down: optRetroachievementActivate
                }

                Item {
                    width: parent.width
                    height: vpx(25)
                }

                Item {
                    width: parent.width
                    height: vpx(25)
                }

                Item {
                    width: parent.width
                    height: vpx(25)
                }

                Item {
                    width: parent.width
                    height: vpx(25)
                }

                Item {
                    width: parent.width
                    height: vpx(25)
                }
            }
        }
    }


    MultivalueBox {
        id: localeBox
        z: 3

        model: api.internal.settings.locales
        index: api.internal.settings.locales.currentIndex

        onClose: content.focus = true
        onSelect: api.internal.settings.locales.currentIndex = index
    }
    MultivalueBox {
        id: themeBox
        z: 3

        model: api.internal.settings.themes
        index: api.internal.settings.themes.currentIndex

        onClose: content.focus = true
        onSelect: api.internal.settings.themes.currentIndex = index
    }
}
