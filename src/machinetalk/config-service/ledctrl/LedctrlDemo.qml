/****************************************************************************
**
** Copyright (C) 2014 Alexander Roessler
** License: LGPL version 2.1
**
** This file is part of QtQuickVcp.
**
** All rights reserved. This program and the accompanying materials
** are made available under the terms of the GNU Lesser General Public License
** (LGPL) version 2.1 which accompanies this distribution, and is available at
** http://www.gnu.org/licenses/lgpl-2.1.html
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
** Contributors:
** Alexander Roessler @ The Cool Tool GmbH <mail DOT aroessler AT gmail DOT com>
**
****************************************************************************/
import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import Machinekit.HalRemote.Controls 1.0
import Machinekit.HalRemote 1.0

HalApplicationWindow {
    property double commandValue: 0.0

    id: main

    width: 600
    height: 400
    name: "ledctrl"
    title: qsTr("LED Control Demo")

    HalColorPicker {
        name: "color"
        anchors.fill: parent
        alphaVisible: false
    }
}
