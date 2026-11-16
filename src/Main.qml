import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    width: 940
    height: 730
    minimumWidth: 800
    minimumHeight: 680
    visible: true
    title: "Cursora"
    color: "#101114"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.primary: "#191b20"
    Material.foreground: "#eceef3"
    Material.roundedScale: theme.radius
    property bool busy: false
    property bool quitting: false

    component ActionButton: Button {
        id: control
        implicitHeight: 44
        leftPadding: 18
        rightPadding: 18
        Material.roundedScale: theme.radius
        background: Rectangle {
            radius: theme.radius
            color: control.highlighted && control.enabled ? theme.accent : control.down ? "#30343e" : "#20232a"
            opacity: control.enabled ? 1 : 0.45
        }
        contentItem: Label {
            text: control.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: !control.enabled ? "#777e8c" : control.highlighted ? theme.accentForeground : "#eceef3"
        }
    }

    function applySelection() {
        if (busy) return;
        busy = true;
        Qt.callLater(function() { backend.apply(); busy = false; });
    }
    function requestQuit() {
        if (backend.dirty) quitDialog.open();
        else { quitting = true; close(); }
    }
    onClosing: function(event) {
        if (!quitting && backend.dirty) { event.accepted = false; quitDialog.open(); }
    }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: win.applySelection() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: { if (backend.canUndo) undoDialog.open(); } }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; onActivated: backend.rescan() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: backend.discard() }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 26
            spacing: 20
            RowLayout {
                spacing: 14
                Rectangle {
                    width: 42; height: 42; radius: Math.min(4, theme.radius); color: theme.accent
                    Image { anchors.centerIn: parent; width: 28; height: 28; source: "qrc:/mark.svg" }
                }
                ColumnLayout {
                    spacing: 2
                    Label { text: "Cursora"; font.pixelSize: 25; font.bold: true }
                    Label { text: qsTr("Your cursor. Your style."); color: "#959ba9"; font.pixelSize: 13 }
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    objectName: "languageButton"
                    text: translator.language.toUpperCase()
                    onClicked: languageMenu.open()
                    Menu {
                        id: languageMenu
                        MenuItem { text: "Русский"; onTriggered: translator.setLanguage("ru") }
                        MenuItem { text: "English"; onTriggered: translator.setLanguage("en") }
                        MenuItem { text: "Dansk"; onTriggered: translator.setLanguage("da") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Follow Omarchy"); onTriggered: translator.setLanguage("") }
                    }
                }
                ToolButton { text: "?"; Accessible.name: qsTr("Keyboard shortcuts"); onClicked: helpDialog.open() }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 22
                ColumnLayout {
                    Layout.preferredWidth: 250
                    Layout.maximumWidth: 280
                    Layout.minimumWidth: 230
                    Layout.fillHeight: true
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("THEMES"); font.pixelSize: 11; font.letterSpacing: 2; color: "#959ba9" }
                        Item { Layout.fillWidth: true }
                        ToolButton { text: "↻"; Accessible.name: qsTr("Refresh themes"); onClicked: backend.rescan() }
                    }
                    ListView {
                        id: themeList
                        objectName: "themeList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: backend.themes
                        keyNavigationEnabled: true
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Keys.onUpPressed: {
                            currentIndex = Math.max(0, currentIndex - 1);
                            backend.selected = model[currentIndex].id;
                            positionViewAtIndex(currentIndex, ListView.Contain);
                        }
                        Keys.onDownPressed: {
                            currentIndex = Math.min(count - 1, currentIndex + 1);
                            backend.selected = model[currentIndex].id;
                            positionViewAtIndex(currentIndex, ListView.Contain);
                        }
                        delegate: Item {
                            required property var modelData
                            required property int index
                            width: themeList.width
                            height: 68
                            Rectangle {
                                anchors.fill: parent
                                color: backend.selected === modelData.id ? "#252931" : "#191b20"
                                border.width: backend.selected === modelData.id ? 1 : 0
                                border.color: theme.accent
                                radius: theme.radius
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 12; spacing: 12
                                    Image {
                                        source: backend.preview(modelData.id, "left_ptr", 48)
                                        Layout.preferredWidth: 38; Layout.preferredHeight: 38
                                        fillMode: Image.PreserveAspectFit
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight; font.pixelSize: 14; font.bold: backend.selected === modelData.id }
                                        Label { text: backend.active === modelData.id ? qsTr("Active") : modelData.builtin ? qsTr("Color collection") : qsTr("Installed theme"); color: backend.active === modelData.id ? theme.accent : "#959ba9"; font.pixelSize: 11 }
                                    }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { backend.selected = modelData.id; themeList.currentIndex = index; themeList.forceActiveFocus(); }
                                Accessible.role: Accessible.Button
                                Accessible.name: modelData.name
                            }
                        }
                        Label {
                            anchors.centerIn: parent; width: parent.width - 20
                            visible: themeList.count === 0
                            text: qsTr("No themes found. Add a theme and refresh the list.")
                            wrapMode: Text.WordWrap; color: "#959ba9"
                        }
                    }
                    ActionButton { text: qsTr("Add a theme…"); Layout.fillWidth: true; onClicked: addDialog.open() }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("PREVIEW"); color: "#959ba9"; font.pixelSize: 11; font.letterSpacing: 2 }
                        Item { Layout.fillWidth: true }
                        Label { text: backend.selected.replace("Cursora-", ""); font.pixelSize: 14; color: theme.accent; elide: Text.ElideRight; Layout.maximumWidth: 240 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 164
                        spacing: 10
                        Repeater {
                            model: [{light: false, label: qsTr("Dark background")}, {light: true, label: qsTr("Light background")}]
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: modelData.light ? "#edf0f5" : "#1b1e25"
                                radius: theme.radius
                                Image {
                                    anchors.centerIn: parent
                                    source: backend.preview(backend.selected, "left_ptr", 72)
                                    width: 76; height: 76; fillMode: Image.PreserveAspectFit
                                }
                                Label { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 13; text: modelData.label; color: modelData.light ? "#616875" : "#959ba9"; font.pixelSize: 11 }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        color: "#191b20"; radius: theme.radius
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 14; spacing: 4
                            Repeater {
                                model: ["left_ptr", "hand2", "xterm", "crosshair", "ew-resize", "watch"]
                                ColumnLayout {
                                    required property string modelData
                                    Layout.fillWidth: true; spacing: 4
                                    Image { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 36; Layout.preferredHeight: 36; source: backend.preview(backend.selected, modelData, 48); fillMode: Image.PreserveAspectFit }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            Label { text: qsTr("Cursor size"); font.pixelSize: 15; Layout.fillWidth: true }
                            SpinBox {
                                objectName: "sizeBox"
                                from: 16; to: 96; stepSize: 1; value: backend.cursorSize
                                editable: true
                                onValueModified: backend.cursorSize = value
                                Accessible.name: qsTr("Cursor size")
                            }
                        }
                        Slider {
                            objectName: "sizeSlider"
                            Layout.fillWidth: true
                            from: 16; to: 96; stepSize: 1; value: backend.cursorSize
                            onMoved: backend.cursorSize = Math.round(value)
                            Accessible.name: qsTr("Cursor size")
                        }
                        RowLayout {
                            Label { text: qsTr("Small"); color: "#959ba9"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Label { text: qsTr("Large"); color: "#959ba9"; font.pixelSize: 11 }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 72
                        color: "#191b20"; radius: theme.radius
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 14; spacing: 16
                            Image { source: backend.preview(backend.selected, "left_ptr", backend.cursorSize); Layout.preferredWidth: backend.cursorSize; Layout.preferredHeight: backend.cursorSize; fillMode: Image.PreserveAspectFit }
                            Label { text: qsTr("Selected size · %1 px").arg((translator.language, backend.cursorSize.toLocaleString(Qt.locale(), 'f', 0))); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#959ba9"; font.pixelSize: 12 }
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2b2f38" }
            RowLayout {
                Layout.fillWidth: true; spacing: 16
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 5
                    Label { text: backend.dirty ? qsTr("Ready to apply") : qsTr("Current cursor"); font.bold: true; font.pixelSize: 13 }
                    Label {
                        objectName: "statusLabel"
                        text: backend.message.length ? backend.message : qsTr("Changes are saved for your next login.")
                        color: backend.failed ? "#ff9292" : "#959ba9"
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 12
                    }
                }
                ActionButton { text: qsTr("Undo"); enabled: backend.canUndo && !win.busy; onClicked: undoDialog.open() }
                ActionButton {
                    objectName: "applyButton"
                    text: win.busy ? qsTr("Applying…") : qsTr("Apply")
                    highlighted: true
                    enabled: !win.busy && themeList.count > 0
                    onClicked: win.applySelection()
                }
            }
        }
    }
    Dialog {
        id: addDialog
        anchors.centerIn: parent
        width: Math.min(540, win.width - 60)
        title: qsTr("Add a cursor theme")
        modal: true
        standardButtons: Dialog.Close
        ColumnLayout {
            width: parent.width; spacing: 18
            Label { Layout.fillWidth: true; text: qsTr("Unpack an Xcursor theme into the themes folder. Each theme should contain a cursors folder. Return here and refresh the list."); wrapMode: Text.WordWrap }
            ActionButton { text: qsTr("Open themes folder"); onClicked: backend.openThemesDirectory() }
            Label { Layout.fillWidth: true; text: qsTr("Six color themes are included. Hyprcursor-only themes need an Xcursor version for application compatibility."); color: "#959ba9"; wrapMode: Text.WordWrap; font.pixelSize: 12 }
        }
    }
    Dialog {
        id: helpDialog
        anchors.centerIn: parent
        width: Math.min(500, win.width - 60)
        title: qsTr("Keyboard shortcuts")
        modal: true
        standardButtons: Dialog.Close
        Label { width: parent.width; text: qsTr("↑ / ↓     Select a theme\nSpace     Apply\nCtrl+Z    Undo the last change\nCtrl+R    Refresh themes\nEsc       Discard selection\nQ         Quit\n?         Keyboard shortcuts"); lineHeight: 1.7; wrapMode: Text.WordWrap }
    }
    Dialog {
        id: quitDialog
        anchors.centerIn: parent; width: 420
        title: qsTr("Discard the selection?")
        modal: true
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label { width: parent.width; text: qsTr("The selected cursor has not been applied yet."); wrapMode: Text.WordWrap }
        onDiscarded: { win.quitting = true; win.close(); }
    }
    Dialog {
        id: undoDialog
        anchors.centerIn: parent; width: 440
        title: qsTr("Restore the previous cursor?")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { width: parent.width; text: qsTr("Restore the settings from before the last application. The current selection will be discarded."); wrapMode: Text.WordWrap }
        onAccepted: { win.busy = true; Qt.callLater(function() { backend.undo(); win.busy = false; }); }
    }
}
