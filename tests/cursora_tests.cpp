#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "cursorimages.h"

class CursoraTests : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
    static bool write(const QString &path, const QByteArray &data) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }
    static QByteArray read(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
    static void theme(const QString &root, const QString &id, const QString &name = {}) {
        write(root + "/" + id + "/index.theme", "[Icon Theme]\nName=" + (name.isEmpty() ? id : name).toUtf8() + "\n");
        write(root + "/" + id + "/cursors/left_ptr", "test cursor");
    }
private slots:
    void initTestCase() {
        qputenv("XDG_CONFIG_HOME", preferences.path().toUtf8());
        qunsetenv("CURSORA_LANGUAGE");
        QQuickStyle::setStyle("Material");
    }
    void discoveryFindsOnlyCursorThemesAndHonorsPrecedence() {
        QTemporaryDir dir;
        theme(dir.path() + "/local", "Example", "Local name");
        theme(dir.path() + "/system", "Example", "System name");
        theme(dir.path() + "/system", "Other");
        theme(dir.path() + "/local", "bad'name");
        write(dir.path() + "/local/Icons/index.theme", "[Icon Theme]\nName=Not cursors\n");
        Backend backend(dir.path(), {dir.path() + "/local", dir.path() + "/system"}, false);
        QCOMPARE(backend.themes().size(), 2);
        QCOMPARE(backend.themes().first().toMap()["name"].toString(), QString("Local name"));
    }
    void iniPreservesUnrelatedSettingsAndSections() {
        const QByteArray before = "# personal\n[Settings]\ngtk-theme-name=MyTheme\ngtk-cursor-theme-size=24\n[Other]\nfoo=bar\n";
        const auto after = Backend::iniValue(before, "Settings", "gtk-cursor-theme-size", "40");
        QVERIFY(after.contains("# personal\n"));
        QVERIFY(after.contains("gtk-theme-name=MyTheme\n"));
        QVERIFY(after.contains("gtk-cursor-theme-size=40\n[Other]\nfoo=bar"));
        QCOMPARE(Backend::iniValue(after, "Settings", "gtk-cursor-theme-size", "40"), after);
    }
    void managedEnvironmentDoesNotDuplicateBlocks() {
        const QByteArray original = "export MY_SETTING=keep\n";
        auto result = Backend::managedBlock(original, "export XCURSOR_SIZE=24\n");
        result = Backend::managedBlock(result, "export XCURSOR_SIZE=40\n");
        QVERIFY(result.startsWith(original));
        QVERIFY(result.contains("XCURSOR_SIZE=40"));
        QVERIFY(!result.contains("XCURSOR_SIZE=24"));
        QCOMPARE(result.count("# >>> cursora >>>"), 1);
    }
    void applyPersistsAndUndoRestoresEveryOriginal() {
        QTemporaryDir dir;
        const QString config = dir.path() + "/.config";
        theme(dir.path() + "/themes", "Example");
        const QByteArray main = "-- my desktop\nrequire(\"hypr.input\")\n";
        const QByteArray gtk = "[Settings]\ngtk-theme-name=Personal\n";
        QVERIFY(write(config + "/hypr/hyprland.lua", main));
        QVERIFY(write(config + "/gtk-3.0/settings.ini", gtk));
        QVERIFY(write(config + "/cursora/settings.json", "{\"theme\":\"Adwaita\",\"size\":40}"));
        Backend backend(dir.path(), {dir.path() + "/themes"}, false);
        backend.setSelected("Example");
        QVERIFY(backend.dirty());
        QVERIFY2(backend.apply(), qPrintable(backend.message()));
        QVERIFY(!backend.dirty());
        QVERIFY(backend.canUndo());
        QVERIFY(read(config + "/hypr/cursora.lua").contains("hl.env(\"XCURSOR_SIZE\", \"40\")"));
        QVERIFY(read(config + "/gtk-3.0/settings.ini").contains("gtk-theme-name=Personal"));
        Backend reopened(dir.path(), {dir.path() + "/themes"}, false);
        QCOMPARE(reopened.selected(), QString("Example"));
        QCOMPARE(reopened.cursorSize(), 40);
        QVERIFY(reopened.canUndo());
        QVERIFY2(reopened.undo(), qPrintable(reopened.message()));
        QCOMPARE(read(config + "/hypr/hyprland.lua"), main);
        QCOMPARE(read(config + "/gtk-3.0/settings.ini"), gtk);
        QVERIFY(!QFileInfo::exists(config + "/hypr/cursora.lua"));
        QVERIFY(!QFileInfo::exists(config + "/gtk-4.0/settings.ini"));
        QVERIFY(!QFileInfo::exists(config + "/uwsm/env"));
        QVERIFY(!reopened.canUndo());
    }
    void repeatedApplyAddsOnlyOneHyprlandHook() {
        QTemporaryDir dir;
        theme(dir.path() + "/themes", "Example");
        Backend backend(dir.path(), {dir.path() + "/themes"}, false);
        backend.setSelected("Example");
        QVERIFY(backend.apply());
        QVERIFY(backend.apply());
        QCOMPARE(read(dir.path() + "/.config/hypr/hyprland.lua").count("require(\"hypr.cursora\")"), 1);
        QVERIFY(backend.undo());
        QCOMPARE(backend.cursorSize(), 24);
        QVERIFY(read(dir.path() + "/.config/hypr/cursora.lua").contains("\"24\""));
    }
    void undoPreservesSubsequentUserEdits() {
        QTemporaryDir dir;
        theme(dir.path() + "/themes", "Example");
        Backend backend(dir.path(), {dir.path() + "/themes"}, false);
        backend.setSelected("Example"); QVERIFY(backend.apply());
        const QString main = dir.path() + "/.config/hypr/hyprland.lua";
        const QByteArray edited = read(main) + "-- later manual edit\n";
        QVERIFY(write(main, edited));
        QVERIFY(!backend.undo());
        QCOMPARE(read(main), edited);
        QVERIFY(QFileInfo::exists(dir.path() + "/.config/hypr/cursora.lua"));
        QVERIFY(backend.message().contains("preserved"));
    }
    void unknownThemeAndSymlinkDoNotModifyConfiguration() {
        QTemporaryDir dir;
        theme(dir.path() + "/themes", "Example");
        const QString main = dir.path() + "/.config/hypr/hyprland.lua";
        QVERIFY(write(main, "original\n"));
        Backend backend(dir.path(), {dir.path() + "/themes"}, false);
        backend.setSelected("bad'); os.execute('oops");
        QVERIFY(!backend.apply());
        QCOMPARE(read(main), QByteArray("original\n"));
        const QString target = dir.path() + "/personal";
        QVERIFY(write(target, "personal\n"));
        QVERIFY(QFile::remove(main)); QVERIFY(QFile::link(target, main));
        backend.setSelected("Example");
        QVERIFY(!backend.apply());
        QCOMPARE(read(target), QByteArray("personal\n"));
        QVERIFY(QFileInfo(main).isSymLink());
    }
    void writeFailureRollsBackEarlierWrites() {
        QTemporaryDir dir;
        theme(dir.path() + "/themes", "Example");
        const QString config = dir.path() + "/.config";
        QVERIFY(write(config + "/cursora/settings.json", "{}"));
        // A regular file where a directory is needed forces a real I/O failure.
        QVERIFY(write(config + "/gtk-4.0", "blocked"));
        Backend backend(dir.path(), {dir.path() + "/themes"}, false);
        backend.setSelected("Example");
        QVERIFY(!backend.apply());
        QCOMPARE(read(config + "/cursora/settings.json"), QByteArray("{}"));
        QVERIFY(!QFileInfo::exists(config + "/gtk-3.0/settings.ini"));
        QVERIFY(!QFileInfo::exists(config + "/hypr/cursora.lua"));
    }
    void sizeOnlyChangePersistsAndUndoRestoresSize() {
        QTemporaryDir dir;
        const QStringList roots{dir.path() + "/themes"};
        theme(roots.first(), "Adwaita");
        Backend backend(dir.path(), roots, false);
        backend.setCursorSize(48); QVERIFY(backend.dirty());
        backend.discard(); QVERIFY(!backend.dirty()); QCOMPARE(backend.cursorSize(), 24);
        backend.setCursorSize(48);
        QVERIFY(backend.apply());
        QCOMPARE(backend.active(), QString("Adwaita"));
        QVERIFY(!backend.dirty());
        Backend reopened(dir.path(), roots, false);
        QCOMPARE(reopened.cursorSize(), 48);
        QVERIFY(reopened.undo());
        QCOMPARE(reopened.cursorSize(), 24);
    }
    void runtimeApplyRetainsSizeChangedOutsideTheApp() {
        QTemporaryDir dir;
        const QByteArray oldPath = qgetenv("PATH"), oldConfig = qgetenv("XDG_CONFIG_HOME"),
            oldState = qgetenv("XDG_STATE_HOME"), oldData = qgetenv("XDG_DATA_HOME"),
            oldSession = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
        const auto restoreEnvironment = qScopeGuard([&] {
            qputenv("PATH", oldPath); qputenv("XDG_CONFIG_HOME", oldConfig);
            if (oldState.isEmpty()) qunsetenv("XDG_STATE_HOME"); else qputenv("XDG_STATE_HOME", oldState);
            if (oldData.isEmpty()) qunsetenv("XDG_DATA_HOME"); else qputenv("XDG_DATA_HOME", oldData);
            if (oldSession.isEmpty()) qunsetenv("HYPRLAND_INSTANCE_SIGNATURE"); else qputenv("HYPRLAND_INSTANCE_SIGNATURE", oldSession);
        });
        const QString bin = dir.path() + "/bin";
        QVERIFY(write(bin + "/size", "24\n"));
        QVERIFY(write(bin + "/gsettings", "#!/bin/sh\nif [ \"$1\" = get ]; then\n"
            "if [ \"$3\" = cursor-theme ]; then printf \"'Adwaita'\\n\"; else cat \"$(dirname \"$0\")/size\"; fi\nfi\n"));
        QVERIFY(write(bin + "/hyprctl", "#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$(dirname \"$0\")/hyprctl-calls\"\n"
            "if [ \"$1\" != configerrors ]; then printf 'ok\\n'; fi\n"));
        QVERIFY(write(bin + "/dbus-update-activation-environment", "#!/bin/sh\nexit 0\n"));
        for (const auto &name : {"gsettings", "hyprctl", "dbus-update-activation-environment"})
            QVERIFY(QFile::setPermissions(bin + "/" + name, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PATH", bin.toUtf8() + ":" + oldPath);
        qputenv("XDG_CONFIG_HOME", (dir.path() + "/.config").toUtf8());
        qputenv("XDG_STATE_HOME", (dir.path() + "/.state").toUtf8());
        qputenv("XDG_DATA_HOME", (dir.path() + "/.data").toUtf8());
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", "test");
        QVERIFY(write(dir.path() + "/.config/hypr/hyprland.lua", "-- test configuration\n"));
        QVERIFY(write(dir.path() + "/.config/cursora/settings.json", "{\"theme\":\"Adwaita\",\"size\":40}"));
        theme(dir.path() + "/themes", "Example");
        Backend backend({}, {dir.path() + "/themes"}, true);
        QCOMPARE(backend.cursorSize(), 24);
        QVERIFY(write(bin + "/size", "42\n"));
        backend.setSelected("Example");
        QVERIFY2(backend.apply(), qPrintable(backend.message()));
        QCOMPARE(backend.cursorSize(), 42);
        QVERIFY(read(dir.path() + "/.config/hypr/cursora.lua").contains("\"42\""));
        QVERIFY(read(dir.path() + "/.config/gtk-3.0/settings.ini").contains("gtk-cursor-theme-size=42"));
        QVERIFY(read(bin + "/hyprctl-calls").contains("--batch setcursor default 42;setcursor Example 42"));
    }
    void themeAndTranslatorFollowReplacedFiles() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QVERIFY(write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\nbackground = \"#101020\"\nforeground = \"#fafafa\"\nselection = \"#353545\"\n"));
        QTRY_COMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.background(), QString("#101020"));
        QCOMPARE(theme.foreground(), QString("#fafafa"));
        QCOMPARE(theme.selection(), QString("#353545"));
        const QString lang = dir.path() + "/language";
        QVERIFY(write(lang, "ru\n"));
        Translator translator(lang);
        QCOMPARE(translator.translate("Main", "Apply"), QString::fromUtf8("Применить"));
        QVERIFY(write(lang, "da\n"));
        QTRY_COMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("Main", "Apply"), QString("Anvend"));
    }
    void realCursorPreviewLoads() {
        CursorImages provider;
        QSize size;
        const QString id = QString::fromLatin1(QUrl::toPercentEncoding("/usr/share/icons/Adwaita/cursors/left_ptr")) + "|48";
        const QImage image = provider.requestImage(id, &size, {});
        QVERIFY(!image.isNull()); QVERIFY(size.width() > 0);
        QVERIFY(!provider.requestImage(QString(id).replace("|", "%7C"), &size, {}).isNull());
        QVERIFY(provider.requestImage("invalid", &size, {}).isNull());
    }
    void qmlSelectionApplyAndLanguageSwitch() {
        QTest::failOnWarning(QRegularExpression(".*"));
        QTemporaryDir dir;
        // Use real installed shapes while keeping every configuration write in a temp home.
        Backend backend(dir.path(), {QCoreApplication::applicationDirPath() + "/../themes", "/usr/share/icons"}, false);
        Theme theme(dir.path());
        const QString language = dir.path() + "/language";
        QVERIFY(write(language, "ru\n"));
        Translator translator(language);
        qApp->installTranslator(&translator);
        QQmlApplicationEngine engine;
        engine.addImageProvider("cursor", new CursorImages);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window); QTest::qWait(150);
        auto *list = window->findChild<QQuickItem *>("themeList");
        QVERIFY(list); list->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Up);
        QVERIFY(backend.selected() != "Adwaita");
        auto *slider = window->findChild<QQuickItem *>("sizeSlider");
        QVERIFY(slider);
        slider->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(backend.cursorSize(), 25);
        auto *apply = window->findChild<QQuickItem *>("applyButton");
        QVERIFY(apply);
        const QPointF buttonPoint = apply->mapToScene(QPointF(apply->width() / 2, apply->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, buttonPoint.toPoint());
        QTRY_VERIFY(backend.canUndo()); QVERIFY(!backend.dirty());
        QCOMPARE(Backend(dir.path(), {}, false).cursorSize(), 25);
        QVERIFY(write(language, "da\n")); QTRY_COMPARE(translator.language(), QString("da"));
        QCOMPARE(apply->property("text").toString(), QString("Anvend"));
        const QImage shot = window->grabWindow(); QVERIFY(!shot.isNull());
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(CursoraTests)
#include "cursora_tests.moc"
