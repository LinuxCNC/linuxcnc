import QtQuick 2.1
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.0
import Machinekit.Controls 1.0
import Machinekit.HalRemote 1.0
import Machinekit.HalRemote.Controls 1.0

HalApplicationWindow {
    id: mainWindow

    name: "test"
    title: qsTr("Test App")
    width: 500
    height: 700
    color: "red"
}
