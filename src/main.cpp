#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "cursorimages.h"

int main(int argc, char *argv[]) {
    const QStringList arguments = [&] { QStringList a; for (int i = 1; i < argc; ++i) a << QString::fromLocal8Bit(argv[i]); return a; }();
    const QString command = arguments.value(0, "open");
    const bool gui = command == "open" || arguments.isEmpty() || command == "--screenshot";
    if (!gui || command == "--screenshot") {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QPA_PLATFORMTHEME", "");
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName("cursora");
    app.setApplicationVersion("0.5.5");
    app.setDesktopFileName("cursora");
    app.setWindowIcon(QIcon::fromTheme("cursora"));
    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    const int lang = arguments.indexOf("--language");
    if (lang >= 0) translator.setLanguage(arguments.value(lang + 1));
    if (command == "monochrome-themes" || command == "build-omarchy-pack") {
        QString error;
        const bool ok = command == "monochrome-themes"
            ? monochromeThemes(arguments.value(1), &error)
            : createOmarchyCursorPack(arguments.value(1), arguments.contains("--light"), &error);
        if (!ok) QTextStream(stderr) << error << '\n';
        return ok ? 0 : 1;
    }
    Backend backend({}, {}, command != "--screenshot");
    if (!gui) {
        const bool json = arguments.contains("--json");
        QJsonObject result;
        bool ok = true;
        if (command == "list") {
            result["themes"] = QJsonArray::fromVariantList(backend.themes());
        } else if (command == "status") {
            result = {{"theme", backend.active()}, {"size", backend.cursorSize()}, {"canUndo", backend.canUndo()}};
        } else if (command == "apply") {
            const int ti = arguments.indexOf("--theme");
            const int si = arguments.indexOf("--size");
            if (si >= 0) {
                ok = false;
                result["error"] = "Cursora now uses the size supplied by the cursor pack. Remove --size.";
            } else if (ti < 0 || arguments.value(ti + 1).isEmpty()) {
                ok = false;
                result["error"] = "Usage: cursora apply --theme NAME [--dry-run] [--json]";
            } else {
                backend.setSelected(arguments.value(ti + 1));
                if (arguments.contains("--dry-run")) {
                    const auto plan = backend.plannedFiles();
                    if (plan.isEmpty()) { ok = false; result["error"] = "Theme is not installed."; }
                    else {
                        QJsonObject files;
                        for (auto it = plan.begin(); it != plan.end(); ++it) files[it.key()] = QString::fromUtf8(it.value());
                        result["files"] = files;
                    }
                } else {
                    ok = backend.apply();
                    result[ok ? "message" : "error"] = backend.message();
                }
            }
        } else if (command == "undo") {
            ok = backend.undo();
            result[ok ? "message" : "error"] = backend.message();
        } else if (command == "skill" && arguments.value(1) == "install") {
            const QString path = QDir::homePath() + "/.codex/skills/cursora/SKILL.md";
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile source(":/agent-skill.md"), target(path);
            ok = source.open(QIODevice::ReadOnly) && target.open(QIODevice::WriteOnly);
            if (ok) { const auto data = source.readAll(); ok = target.write(data) == data.size(); }
            result[ok ? "path" : "error"] = ok ? path : "Cannot install Cursora skill.";
        } else if (command == "--help" || command == "help") {
            QTextStream(stdout) << "Cursora — cursor packs for Omarchy\n"
                "cursora open [--language ru|en|da]\n"
                "cursora list|status [--json]\n"
                "cursora apply --theme NAME [--dry-run] [--json]\n"
                "cursora undo [--json]\ncursora skill install [--json]\n";
            return 0;
        } else { ok = false; result["error"] = "Unknown command. Run cursora --help."; }
        result["ok"] = ok;
        if (json) QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
        else if (ok) QTextStream(stdout) << QJsonDocument(result).toJson();
        if (!ok) QTextStream(stderr) << result["error"].toString() << '\n';
        return ok ? 0 : 1;
    }
    Theme theme(command == "--screenshot" ? "/tmp/cursora-screenshot-theme" : QString());
    QQmlApplicationEngine engine;
    engine.addImageProvider("cursor", new CursorImages);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (command == "--screenshot") {
        backend.setSelected("Cursora-Omarchy-Terminal");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(1200, &app, [window, &app, arguments] {
            const bool ok = window->grabWindow().save(arguments.value(1));
            app.exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
