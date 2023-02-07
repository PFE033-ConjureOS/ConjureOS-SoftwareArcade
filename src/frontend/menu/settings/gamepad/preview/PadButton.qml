// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
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


import QtQuick 2.12

Item {
    property string shortName
    property bool pressed: false

    height: pieceImage.height

    Image {
        id: pieceImage
        z: 100
        width: parent.width

        fillMode: Image.PreserveAspectFit
        source: "qrc:/frontend/assets/gamepad/" + shortName + ".svg"
        sourceSize {
            width: 64
            height: 64
        }
    }

    Rectangle {
        id: highlight
        color: {
				if (pressed) return "blue";
				else if (root.recordingField !== null ) return "#c33";
				else return themeColor.underline;
		}
        anchors.fill: parent
        radius: width * 0.5

        // FIXME: this is not really nice, but makes the code shorter
        visible: pressed || padContainer.currentButton === shortName
    }
}