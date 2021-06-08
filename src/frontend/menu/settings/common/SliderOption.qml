// Pegasus Frontend
//
// Created by BozoTheGeek 19/04/2021
//

import QtQuick 2.15
import QtQuick.Controls 2.15


FocusScope {
    id: root

    property alias label: label.text
    property alias note: sublabel.text
    
    property alias value: value.text 
    
    property alias slidervalue : slider.value
    property alias max : slider.from
    property alias min : slider.to
    
    property int fontSize: vpx(22)
    property int horizontalPadding: vpx(30)

    signal activate()


    width: parent.width
    height: labelContainer.height + fontSize * 1.25

    Keys.onPressed: {
        if (api.keys.isAccept(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.activate();
        }
    }
    Keys.onLeftPressed: slider.value > min ? slider.value = slider.value - 1 : min
    Keys.onRightPressed: slider.value < max ? slider.value = slider.value + 1 : max       

    Rectangle {
        id: underline

        width: parent.width
        height: vpx(3)
        anchors.bottom: parent.bottom

        color: themeColor.underline
        visible: parent.focus || mouseArea.containsMouse
    }

    Column {
        id: labelContainer
        anchors {
            left: parent.left; leftMargin: horizontalPadding
            right: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
        }

        spacing: fontSize * 0.25
        height: label.height + (sublabel.text ? spacing + sublabel.height : 0)


        Text {
            id: label

            color:themeColor.textLabel
            font.pixelSize: fontSize
            font.family: globalFonts.sans
        }

        Text {
            id: sublabel

            color: themeColor.textSublabel
            font.pixelSize: fontSize * 0.8
            font.family: globalFonts.sans
            font.italic: true
        }
    }
    
    Slider {
        id: slider
        width: 300
        rotation: -180
        orientation: Qt.Horizontal
        anchors.right: parent.right
        anchors.rightMargin: horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
    }

    Text {
        id: value

        anchors.right: parent.right
        anchors.rightMargin: horizontalPadding + slider.width
        anchors.verticalCenter: parent.verticalCenter

        color: themeColor.textValue
        font.pixelSize: fontSize
        font.family: globalFonts.sans
    }    
    

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: activate()
        cursorShape: Qt.PointingHandCursor
    }
}
