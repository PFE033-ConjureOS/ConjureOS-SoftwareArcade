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
//
// created by BozoTheGeek 05/02/2022
//

import QtQuick 2.12


GenericOkCancelDialog
{
    focus: true

    title: qsTr("Restart") + api.tr
    message: qsTr("Pegasus will restart. Are you sure?") + api.tr
//    symbol: "\u21BB"

    onAccept: {
        api.memory.unset("repoStatusRefreshTime");
        api.internal.system.restart();
    }
}
