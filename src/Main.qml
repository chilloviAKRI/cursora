import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    width: 920; height: 650
    minimumWidth: 780; minimumHeight: 570
    visible: true
    title: "Cursora"
    color: theme.background
    font.family: "monospace"
    font.pixelSize: 13
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius
    property bool busy: false
    property bool quitting: false
    readonly property color textColor: theme.foreground
    readonly property color lineColor: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.22)
    readonly property color quietColor: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.58)

    function titleFor(id) { return (translator.language, backend.themeTitle(id)); }
    function descriptionFor(id) { return (translator.language, backend.themeDescription(id)); }
    function applySelection() {
        if (busy || themeList.count === 0) return;
        busy = true;
        Qt.callLater(function() { backend.apply(); busy = false; });
    }
    function requestQuit() {
        if (backend.dirty) quitDialog.open();
        else { quitting = true; close(); }
    }
    function syncSelection() {
        for (let i = 0; i < backend.themes.length; i++) {
            if (backend.themes[i].id === backend.selected) {
                if (themeList.currentIndex !== i) {
                    themeList.currentIndex = i;
                    themeList.positionViewAtIndex(i, ListView.Contain);
                }
                return;
            }
        }
    }
    onClosing: function(event) {
        if (!quitting && backend.dirty) { event.accepted = false; quitDialog.open(); }
    }
    Component.onCompleted: syncSelection()
    Connections { target: backend; function onChanged() { win.syncSelection(); } }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: win.applySelection() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: { if (backend.canUndo) undoDialog.open(); } }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; onActivated: backend.rescan() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: backend.discard() }

    component TextButton: Button {
        id: control
        implicitHeight: 34
        leftPadding: 12; rightPadding: 12
        hoverEnabled: true
        Material.roundedScale: theme.radius
        background: Rectangle {
            radius: theme.radius
            color: control.highlighted && control.enabled ? theme.foreground
                : control.down || control.hovered || control.activeFocus ? theme.selection : "transparent"
            border.width: 1
            border.color: control.enabled ? win.lineColor : Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.1)
        }
        contentItem: Label {
            text: control.text
            font: control.font
            color: !control.enabled ? Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.3)
                : control.highlighted ? theme.background : theme.foreground
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    component Sheet: Dialog {
        id: sheet
        anchors.centerIn: parent
        width: Math.min(540, win.width - 64)
        modal: true
        padding: 22
        Material.roundedScale: theme.radius
        background: Rectangle { color: theme.background; border.color: theme.foreground; border.width: 1; radius: theme.radius }
        header: Label {
            text: sheet.title
            color: theme.foreground
            font.bold: true; font.pixelSize: 15
            padding: 22; bottomPadding: 16
            wrapMode: Text.WordWrap
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 24
            spacing: 18
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                Label { text: ">"; color: theme.accent; font.pixelSize: 21; font.bold: true }
                Label { text: "cursora"; color: theme.foreground; font.pixelSize: 20; font.bold: true }
                Label { text: "/ " + qsTr("cursor packs"); color: win.quietColor; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                TextButton {
                    objectName: "languageButton"
                    text: translator.language.toUpperCase()
                    onClicked: languageMenu.open()
                    Menu {
                        id: languageMenu
                        Material.roundedScale: theme.radius
                        background: Rectangle { color: theme.background; border.width: 1; border.color: win.lineColor; radius: theme.radius }
                        MenuItem { text: "Русский"; onTriggered: translator.setLanguage("ru") }
                        MenuItem { text: "English"; onTriggered: translator.setLanguage("en") }
                        MenuItem { text: "Dansk"; onTriggered: translator.setLanguage("da") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Follow Omarchy"); onTriggered: translator.setLanguage("") }
                    }
                }
                TextButton { text: "?"; Accessible.name: qsTr("Keyboard shortcuts"); onClicked: helpDialog.open() }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: win.lineColor }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                spacing: 22
                ColumnLayout {
                    Layout.preferredWidth: 266; Layout.minimumWidth: 240; Layout.maximumWidth: 280
                    Layout.fillHeight: true
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("PACKS"); color: win.quietColor; font.pixelSize: 11; font.letterSpacing: 1.3 }
                        Item { Layout.fillWidth: true }
                        TextButton { text: "↻"; implicitHeight: 28; Accessible.name: qsTr("Refresh themes"); onClicked: backend.rescan() }
                    }
                    ListView {
                        id: themeList
                        objectName: "themeList"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: backend.themes
                        clip: true
                        spacing: 2
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Keys.onUpPressed: {
                            if (!count) return;
                            currentIndex = Math.max(0, currentIndex - 1);
                            backend.selected = model[currentIndex].id;
                        }
                        Keys.onDownPressed: {
                            if (!count) return;
                            currentIndex = Math.min(count - 1, currentIndex + 1);
                            backend.selected = model[currentIndex].id;
                        }
                        delegate: Item {
                            id: row
                            required property var modelData
                            required property int index
                            width: themeList.width; height: 54
                            readonly property bool chosen: backend.selected === modelData.id
                            Rectangle {
                                anchors.fill: parent
                                color: row.chosen ? theme.selection : rowMouse.containsMouse ? Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.04) : "transparent"
                                radius: theme.radius
                                border.width: row.chosen ? 1 : 0
                                border.color: theme.accent
                            }
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 11
                                Image {
                                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                    source: backend.preview(row.modelData.id, "left_ptr", row.modelData.nativeSize)
                                    fillMode: Image.PreserveAspectFit
                                }
                                Label {
                                    text: win.titleFor(row.modelData.id)
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: 12
                                    color: theme.foreground
                                }
                                Label { text: backend.active === row.modelData.id ? "●" : row.chosen ? "‹" : ""; color: theme.accent; font.pixelSize: 12 }
                            }
                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent; hoverEnabled: true
                                onClicked: { backend.selected = row.modelData.id; themeList.forceActiveFocus(); }
                                Accessible.role: Accessible.Button
                                Accessible.name: win.titleFor(row.modelData.id)
                            }
                        }
                        Label {
                            anchors.centerIn: parent; width: parent.width - 16
                            visible: themeList.count === 0
                            text: qsTr("No themes found. Add a theme and refresh the list.")
                            wrapMode: Text.WordWrap; color: win.quietColor
                        }
                    }
                    TextButton { text: "+ " + qsTr("Add a pack"); Layout.fillWidth: true; onClicked: addDialog.open() }
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: win.lineColor }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 7
                        Label { text: win.titleFor(backend.selected); color: theme.foreground; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: win.descriptionFor(backend.selected); color: win.quietColor; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Layout.minimumHeight: 160; Layout.maximumHeight: 220
                        spacing: 10
                        Repeater {
                            model: [{light: false, label: qsTr("DARK")}, {light: true, label: qsTr("LIGHT")}]
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true; Layout.fillHeight: true
                                color: modelData.light ? "#eeeeee" : "#151515"
                                border.width: 1; border.color: win.lineColor
                                radius: theme.radius
                                Image {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: -12
                                    readonly property int magnification: Math.max(1, Math.min(3,
                                        Math.floor((parent.height - 40) / Math.max(1, implicitHeight)),
                                        Math.floor((parent.width - 16) / Math.max(1, implicitWidth))))
                                    width: implicitWidth * magnification
                                    height: implicitHeight * magnification
                                    source: backend.preview(backend.selected, "left_ptr", backend.themeSize(backend.selected))
                                    smooth: false
                                }
                                Label {
                                    anchors.bottom: parent.bottom; anchors.left: parent.left
                                    anchors.margins: 12
                                    text: modelData.label; font.pixelSize: 10; font.letterSpacing: 1
                                    color: modelData.light ? "#666666" : "#999999"
                                }
                            }
                        }
                    }
                    Label { text: qsTr("CURSOR SHAPES"); color: win.quietColor; font.pixelSize: 10; font.letterSpacing: 1.2 }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 94
                        color: "transparent"; border.width: 1; border.color: win.lineColor; radius: theme.radius
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 2
                            Repeater {
                                model: [{shape:"left_ptr", label:qsTr("Arrow")}, {shape:"hand2", label:qsTr("Link")},
                                    {shape:"xterm", label:qsTr("Text")}, {shape:"crosshair", label:qsTr("Cross")},
                                    {shape:"ew-resize", label:qsTr("Resize")}, {shape:"watch", label:qsTr("Wait")}]
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true; spacing: 9
                                    Image { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 32; Layout.preferredHeight: 32; source: backend.preview(backend.selected, modelData.shape, backend.themeSize(backend.selected)); fillMode: Image.PreserveAspectFit }
                                    Label { text: modelData.label; Layout.alignment: Qt.AlignHCenter; color: win.quietColor; font.pixelSize: 9 }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true; Layout.maximumHeight: 8 }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: win.lineColor }
            RowLayout {
                Layout.fillWidth: true; spacing: 18
                Label {
                    objectName: "statusLabel"
                    text: backend.failed || !backend.dirty && backend.message.length ? backend.message
                        : backend.dirty ? qsTr("Press Apply to save changes.") : qsTr("● Active pack")
                    color: backend.failed ? theme.accent : win.quietColor
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
                }
                TextButton { text: qsTr("Undo"); enabled: backend.canUndo && !win.busy; onClicked: undoDialog.open() }
                TextButton {
                    objectName: "applyButton"
                    text: win.busy ? qsTr("Applying…") : qsTr("Apply")
                    highlighted: true
                    enabled: !win.busy && themeList.count > 0
                    onClicked: win.applySelection()
                }
            }
            Label { text: qsTr("↑↓ select   Space apply   Ctrl+Z undo   ? help"); color: win.quietColor; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }
    Sheet {
        id: addDialog
        title: qsTr("Add a cursor pack")
        contentItem: ColumnLayout {
            spacing: 18
            Label { Layout.fillWidth: true; text: qsTr("Unpack an Xcursor theme into the themes folder. Each theme should contain a cursors folder. Return here and refresh the list."); color: theme.foreground; wrapMode: Text.WordWrap }
            TextButton { text: qsTr("Open themes folder"); onClicked: backend.openThemesDirectory() }
        }
        footer: DialogButtonBox {
            TextButton { text: qsTr("Close"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onRejected: addDialog.reject()
        }
    }
    Sheet {
        id: helpDialog
        title: qsTr("Keyboard shortcuts")
        contentItem: Label { text: qsTr("↑ / ↓     Select a pack\nSpace     Apply\nCtrl+Z    Undo the last change\nCtrl+R    Refresh packs\nEsc       Discard selection\nQ         Quit\n?         Keyboard shortcuts"); color: theme.foreground; lineHeight: 1.7; wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            TextButton { text: qsTr("Close"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            onRejected: helpDialog.reject()
        }
    }
    Sheet {
        id: quitDialog
        title: qsTr("Discard the selection?")
        contentItem: Label { text: qsTr("The selected cursor has not been applied yet."); color: theme.foreground; wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            TextButton { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            TextButton { text: qsTr("Discard"); DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole }
            onRejected: quitDialog.reject()
            onDiscarded: { win.quitting = true; win.close(); }
        }
    }
    Sheet {
        id: undoDialog
        title: qsTr("Restore the previous cursor?")
        contentItem: Label { text: qsTr("Restore the settings from before the last application. The current selection will be discarded."); color: theme.foreground; wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            TextButton { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            TextButton { text: qsTr("Restore"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            onRejected: undoDialog.reject()
            onAccepted: { undoDialog.accept(); win.busy = true; Qt.callLater(function() { backend.undo(); win.busy = false; }); }
        }
    }
}
