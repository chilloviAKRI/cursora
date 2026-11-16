#include "backend.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QSet>
#include <algorithm>

namespace {
QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QString hash(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString unquote(QString text) {
    if (text.startsWith('\'') && text.endsWith('\'')) text = text.mid(1, text.size() - 2);
    return text;
}
QString luaString(const QString &value) {
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}
}

Backend::Backend(const QString &home, const QStringList &roots, bool runtime)
    : m_home(home.isEmpty() ? QDir::homePath() : home), m_runtime(runtime) {
    m_config = home.isEmpty() ? qEnvironmentVariable("XDG_CONFIG_HOME", m_home + "/.config") : m_home + "/.config";
    m_state = home.isEmpty() ? qEnvironmentVariable("XDG_STATE_HOME", m_home + "/.local/state") : m_home + "/.local/state";
    m_data = home.isEmpty() ? qEnvironmentVariable("XDG_DATA_HOME", m_home + "/.local/share") : m_home + "/.local/share";
    m_roots = roots;
    if (m_roots.isEmpty()) {
        m_roots = {m_data + "/icons", m_home + "/.icons"};
        for (const auto &path : qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(':', Qt::SkipEmptyParts))
            m_roots << path + "/icons";
        const QString development = QCoreApplication::applicationDirPath() + "/themes";
        if (QFileInfo::exists(development)) m_roots.prepend(development);
    }
    const auto saved = QJsonDocument::fromJson(read(config() + "/cursora/settings.json")).object();
    if (!saved.isEmpty()) {
        m_active = saved.value("theme").toString();
        m_activeSize = qBound(16, saved.value("size").toInt(24), 96);
    }
    if (runtime) {
        bool ok = false;
        const QString liveTheme = unquote(run("gsettings", {"get", "org.gnome.desktop.interface", "cursor-theme"}, &ok));
        if (ok) m_active = liveTheme;
        if (!ok) m_active = qEnvironmentVariable("XCURSOR_THEME", "Adwaita");
        const int size = run("gsettings", {"get", "org.gnome.desktop.interface", "cursor-size"}, &ok).toInt();
        if (ok && size > 0) m_activeSize = size;
    }
    if (m_active.isEmpty()) m_active = "Adwaita";
    m_selected = m_active;
    m_size = m_activeSize;
    m_lastBackup = QString::fromUtf8(read(state() + "/cursora/last-backup")).trimmed();
    rescan();
}

QString Backend::config() const { return m_config; }
QString Backend::state() const { return m_state; }
QString Backend::themesDirectory() const { return m_data + "/icons"; }
bool Backend::canUndo() const { return QFileInfo::exists(m_lastBackup + "/manifest.json"); }
void Backend::setSelected(const QString &value) {
    if (m_selected == value) return;
    m_selected = value;
    emit changed();
}

void Backend::rescan() {
    QVariantList themes;
    QSet<QString> seen;
    const QJsonObject packs = QJsonDocument::fromJson(read(":/packs.json")).object();
    const QSet<QString> retired{ "Cursora-Amber", "Cursora-Ice", "Cursora-Mint", "Cursora-Ocean", "Cursora-Rose", "Cursora-Violet" };
    static const QRegularExpression valid("^[A-Za-z0-9][A-Za-z0-9_.+ -]{0,100}$");
    for (const auto &root : m_roots) {
        for (const auto &entry : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString id = entry.fileName();
            if (seen.contains(id) || retired.contains(id) || !valid.match(id).hasMatch()) continue;
            const QString path = entry.absoluteFilePath();
            if (!QFileInfo::exists(path + "/cursors/left_ptr") && !QFileInfo::exists(path + "/cursors/default")) continue;
            seen.insert(id);
            QSettings index(path + "/index.theme", QSettings::IniFormat);
            const QString name = index.value("Icon Theme/Name", id).toString();
            themes << QVariantMap{{"id", id}, {"name", name}, {"path", path}, {"builtin", packs.contains(id)}, {"pack", packs.value(id).toObject().toVariantMap()}};
        }
    }
    std::sort(themes.begin(), themes.end(), [](const QVariant &a, const QVariant &b) {
        const auto x = a.toMap(), y = b.toMap();
        if (x["builtin"].toBool() != y["builtin"].toBool()) return x["builtin"].toBool();
        return QString::localeAwareCompare(x["name"].toString(), y["name"].toString()) < 0;
    });
    m_themes = themes;
    if (m_selected == "default") {
        // Most installations make default an inheritance-only alias for Adwaita.
        for (const auto &item : themes) if (item.toMap()["id"] == "Adwaita") {
            m_selected = "Adwaita";
            if (m_active == "default") m_active = "Adwaita";
            break;
        }
    }
    emit changed();
}

QString Backend::themeTitle(const QString &id) const {
    for (const auto &value : m_themes) {
        const auto theme = value.toMap();
        if (theme["id"].toString() != id) continue;
        const auto pack = theme["pack"].toMap();
        if (pack.isEmpty()) return theme["name"].toString();
        return tr("%1 · %2").arg(pack["family"].toString(), pack["tone"].toString() == "black" ? tr("Black") : tr("White"));
    }
    return id;
}

QString Backend::themeDescription(const QString &id) const {
    for (const auto &value : m_themes) {
        const auto theme = value.toMap();
        if (theme["id"].toString() != id) continue;
        const QString family = theme["pack"].toMap()["family"].toString();
        if (family == "Bibata") return tr("Soft, rounded shapes");
        if (family == "Phinger") return tr("Compact pointer, expressive hands");
        if (family == "BreezeX") return tr("Sharp, angular shapes");
    }
    return tr("Installed cursor pack");
}

QString Backend::preview(const QString &id, const QString &shape, int size) const {
    for (const auto &item : m_themes) {
        const auto map = item.toMap();
        if (map["id"].toString() != id) continue;
        const QString directory = map["path"].toString() + "/cursors/";
        QString path = directory + shape;
        if (!QFileInfo::exists(path)) path = directory + "left_ptr";
        if (!QFileInfo::exists(path)) path = directory + "default";
        return "image://cursor/" + QString::fromLatin1(QUrl::toPercentEncoding(path)) + "|" + QString::number(size);
    }
    return {};
}

QByteArray Backend::iniValue(QByteArray data, const QString &section, const QString &key, const QString &value) {
    QStringList lines = QString::fromUtf8(data).split('\n');
    int begin = -1, end = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line == "[" + section + "]") { begin = i; continue; }
        if (begin >= 0 && line.startsWith('[') && line.endsWith(']')) { end = i; break; }
    }
    if (begin < 0) {
        if (!lines.last().isEmpty()) lines << "";
        lines << "[" + section + "]" << key + "=" + value << "";
    } else {
        bool found = false;
        for (int i = end - 1; i > begin; --i) {
            if (lines[i].section('=', 0, 0).trimmed() == key) {
                if (!found) { lines[i] = key + "=" + value; found = true; }
                else lines.removeAt(i);
            }
        }
        if (!found) lines.insert(end, key + "=" + value);
    }
    return lines.join('\n').toUtf8();
}

QByteArray Backend::managedBlock(QByteArray data, const QByteArray &content) {
    const QByteArray begin = "# >>> cursora >>>", end = "# <<< cursora <<<";
    const int first = data.indexOf(begin), last = data.indexOf(end, qMax(0, first));
    if (first >= 0 && last >= first) {
        int count = last + end.size() - first;
        if (data.mid(first + count, 1) == "\n") ++count;
        data.remove(first, count);
    }
    if (!data.isEmpty() && !data.endsWith('\n')) data += '\n';
    return data + begin + '\n' + content + end + '\n';
}

QMap<QString, QByteArray> Backend::plannedFiles() const {
    QMap<QString, QByteArray> files;
    const QString main = config() + "/hypr/hyprland.lua";
    QByteArray contents = read(main);
    const QByteArray hook = "require(\"hypr.cursora\")";
    if (!QRegularExpression(R"(require\s*\(?\s*["']hypr\.cursora["'])").match(QString::fromUtf8(contents)).hasMatch()) {
        if (!contents.endsWith('\n')) contents += '\n';
        contents += "\n-- Cursor preferences managed by Cursora.\n" + hook + '\n';
    }
    files[main] = contents;
    const QString quoted = luaString(m_selected), size = QString::number(m_size);
    files[config() + "/hypr/cursora.lua"] = QString(
        "-- Managed by Cursora. Change the cursor in the application.\n"
        "hl.env(\"XCURSOR_THEME\", %1)\nhl.env(\"XCURSOR_SIZE\", \"%2\")\n"
        "hl.env(\"HYPRCURSOR_THEME\", %1)\nhl.env(\"HYPRCURSOR_SIZE\", \"%2\")\n"
        "hl.config({ cursor = { enable_hyprcursor = false } })\n"
        "hl.exec_cmd(\"hyprctl setcursor \" .. o.shell_quote(%1) .. \" %2\")\n").arg(quoted, size).toUtf8();
    for (const auto &version : {"gtk-3.0", "gtk-4.0"}) {
        const QString path = config() + "/" + version + "/settings.ini";
        QByteArray ini = iniValue(read(path), "Settings", "gtk-cursor-theme-name", m_selected);
        files[path] = iniValue(ini, "Settings", "gtk-cursor-theme-size", size);
    }
    const QString xdefault = m_data + "/icons/default/index.theme";
    files[xdefault] = iniValue(read(xdefault), "Icon Theme", "Inherits", m_selected);
    const QString env = config() + "/uwsm/env", hyprenv = config() + "/uwsm/env-hyprland";
    files[env] = managedBlock(read(env), QString("export XCURSOR_THEME='%1'\nexport XCURSOR_SIZE='%2'\n").arg(m_selected, size).toUtf8());
    files[hyprenv] = managedBlock(read(hyprenv), QString("export HYPRCURSOR_THEME='%1'\nexport HYPRCURSOR_SIZE='%2'\n").arg(m_selected, size).toUtf8());
    files[config() + "/cursora/settings.json"] = QJsonDocument(QJsonObject{{"theme", m_selected}, {"size", m_size}}).toJson();
    return files;
}

QString Backend::run(const QString &program, const QStringList &args, bool *ok) const {
    QProcess process;
    process.start(program, args);
    const bool started = process.waitForStarted(1000);
    bool success = started && process.waitForFinished(4000);
    if (!success && process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(1000); }
    success = success && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (ok) *ok = success;
    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return success ? output : (error.isEmpty() ? (output.isEmpty() ? process.errorString() : output) : error);
}

bool Backend::writeFile(const QString &path, const QByteArray &data, QString *error) const {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { *error = tr("Cannot create folder: %1").arg(QFileInfo(path).absolutePath()); return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = tr("Cannot save %1: %2").arg(path, file.errorString()); return false;
    }
    return true;
}

bool Backend::fail(const QString &message) { m_message = message; m_failed = true; emit changed(); return false; }

bool Backend::applyRuntime(QString *error, const QString &overrideTheme, int overrideSize) {
    if (!m_runtime) return true;
    const QString selected = overrideTheme.isEmpty() ? m_selected : overrideTheme;
    const int size = overrideSize > 0 ? overrideSize : m_size;
    bool ok = false;
    run("hyprctl", {"reload"}, &ok);
    if (!ok) { *error = tr("Hyprland could not reload its configuration."); return false; }
    const QString errors = run("hyprctl", {"configerrors"}, &ok);
    if (!ok || !errors.trimmed().isEmpty()) { *error = tr("Hyprland configuration error: %1").arg(errors); return false; }
    // Reloading cannot unset an hl.env value left by a removed module. Update the
    // compositor's launch environment explicitly, including when undoing a change.
    const QString envCode = QString("hl.env(\"XCURSOR_THEME\", %1); hl.env(\"HYPRCURSOR_THEME\", %1); "
        "hl.env(\"XCURSOR_SIZE\", \"%2\"); hl.env(\"HYPRCURSOR_SIZE\", \"%2\")").arg(luaString(selected), QString::number(size));
    const QString envResult = run("hyprctl", {"eval", envCode}, &ok);
    if (!ok || envResult.contains("error", Qt::CaseInsensitive)) {
        *error = tr("Cannot update the application environment: %1").arg(envResult); return false;
    }
    for (const auto &setting : {qMakePair(QString("cursor-theme"), QString("'%1'").arg(selected)),
                                qMakePair(QString("cursor-size"), QString::number(size))}) {
        const QString result = run("gsettings", {"set", "org.gnome.desktop.interface", setting.first, setting.second}, &ok);
        if (!ok) { *error = tr("Cannot update desktop cursor settings: %1").arg(result); return false; }
    }
    // Hyprland caches Xcursor images by theme name and size. A package update
    // can replace a theme without changing either, so invalidate that cache
    // and select the requested theme in one compositor transaction.
    const QString refresh = selected == "default" ? "Adwaita" : "default";
    const QString batch = QString("setcursor %1 %2;setcursor %3 %2").arg(refresh, QString::number(size), selected);
    const QString cursorResult = run("hyprctl", {"--batch", batch}, &ok);
    if (!ok || cursorResult.contains("error", Qt::CaseInsensitive) || cursorResult.contains("failed", Qt::CaseInsensitive)) {
        *error = tr("Cannot apply the cursor: %1").arg(cursorResult); return false;
    }
    const QString result = run("dbus-update-activation-environment", {"--systemd",
        "XCURSOR_THEME=" + selected, "XCURSOR_SIZE=" + QString::number(size),
        "HYPRCURSOR_THEME=" + selected, "HYPRCURSOR_SIZE=" + QString::number(size)}, &ok);
    if (!ok) { *error = tr("Cannot update the application environment: %1").arg(result); return false; }
    return true;
}

bool Backend::apply() {
    bool exists = false;
    for (const auto &item : m_themes) if (item.toMap()["id"].toString() == m_selected) exists = true;
    if (!exists) return fail(tr("Select an installed cursor theme."));
    if (m_runtime) {
        if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) return fail(tr("Open Cursora inside a Hyprland session."));
        if (!QFileInfo::exists(config() + "/hypr/hyprland.lua")) return fail(tr("This utility requires Omarchy with a Lua Hyprland configuration."));
        for (const auto &tool : {"hyprctl", "gsettings", "dbus-update-activation-environment"})
            if (QStandardPaths::findExecutable(tool).isEmpty()) return fail(tr("Required command is missing: %1").arg(tool));
        bool ok = false;
        const QString errors = run("hyprctl", {"configerrors"}, &ok);
        if (!ok || !errors.isEmpty()) return fail(tr("Fix the existing Hyprland configuration errors first: %1").arg(errors));
    }
    // The utility changes appearance only. Re-read the live size so an external
    // size change made while this window was open is preserved as well.
    if (m_runtime) {
        bool ok = false;
        const int size = run("gsettings", {"get", "org.gnome.desktop.interface", "cursor-size"}, &ok).toInt();
        if (!ok || size <= 0) return fail(tr("Cannot read the current cursor size."));
        m_size = size;
    }
    const auto files = plannedFiles();
    const QString backup = state() + "/cursora/backups/" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-") + QUuid::createUuid().toString(QUuid::Id128).left(8);
    QString error;
    QJsonArray entries;
    for (auto it = files.begin(); it != files.end(); ++it) {
        // Refuse symlinked configuration files instead of replacing their targets.
        if (QFileInfo(it.key()).isSymLink()) return fail(tr("Configuration is a symbolic link; edit it manually: %1").arg(it.key()));
        QFile old(it.key());
        const bool existed = old.exists();
        if (existed && !old.open(QIODevice::ReadOnly)) return fail(tr("Cannot read %1: %2").arg(it.key(), old.errorString()));
        const QByteArray original = existed ? old.readAll() : QByteArray();
        entries.append(QJsonObject{{"path", it.key()}, {"existed", existed}, {"original", QString::fromLatin1(original.toBase64())}, {"after", hash(it.value())}});
    }
    QString runtimeTheme = m_active;
    int runtimeSize = m_activeSize;
    if (m_runtime) {
        bool ok = false;
        runtimeTheme = unquote(run("gsettings", {"get", "org.gnome.desktop.interface", "cursor-theme"}, &ok));
        if (!ok) return fail(tr("Cannot update desktop cursor settings: %1").arg(runtimeTheme));
        const QString value = run("gsettings", {"get", "org.gnome.desktop.interface", "cursor-size"}, &ok);
        runtimeSize = value.toInt();
        if (!ok || runtimeSize <= 0) return fail(tr("Cannot update desktop cursor settings: %1").arg(value));
    }
    const QJsonObject manifest{{"files", entries}, {"theme", m_active}, {"size", m_activeSize},
        {"runtimeTheme", runtimeTheme}, {"runtimeSize", runtimeSize}};
    if (!writeFile(backup + "/manifest.json", QJsonDocument(manifest).toJson(), &error)) return fail(error);
    QFile::setPermissions(backup, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile::setPermissions(backup + "/manifest.json", QFile::ReadOwner | QFile::WriteOwner);
    bool success = true;
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (!writeFile(it.key(), it.value(), &error)) { success = false; break; }
    }
    if (success) success = applyRuntime(&error);
    if (success) success = writeFile(state() + "/cursora/last-backup", backup.toUtf8(), &error);
    if (!success) {
        const QString requested = m_selected;
        const int size = m_size;
        QString restoreError;
        const bool restored = restore(backup, &restoreError, false);
        m_selected = m_active;
        m_size = m_activeSize;
        QString runtimeError;
        const bool runtimeRestored = applyRuntime(&runtimeError, runtimeTheme, runtimeSize);
        m_selected = requested;
        m_size = size;
        return fail(error + (restored && runtimeRestored ? tr(" Previous settings restored.")
            : tr(" Recovery needs attention: %1 %2. Backup: %3").arg(restoreError, runtimeError, backup)));
    }
    m_active = m_selected;
    m_activeSize = m_size;
    m_lastBackup = backup;
    m_failed = false;
    m_message = tr("Cursor applied. Some open applications may need to be restarted.");
    emit changed();
    return true;
}

bool Backend::restore(const QString &directory, QString *error, bool verify) {
    const auto object = QJsonDocument::fromJson(read(directory + "/manifest.json")).object();
    if (!object.contains("files")) { *error = tr("Backup is missing or damaged."); return false; }
    const auto files = object["files"].toArray();
    for (const auto &value : files) {
        const auto entry = value.toObject();
        const QString path = entry["path"].toString();
        if (!(path.startsWith(config() + "/") || path.startsWith(m_data + "/icons/")) || path.contains("/../")) {
            *error = tr("Backup contains an invalid path."); return false;
        }
        if (verify && (QFileInfo(path).isSymLink() || hash(read(path)) != entry["after"].toString())) {
            *error = tr("This file changed after applying the cursor: %1. Your edits were preserved.").arg(path); return false;
        }
    }
    for (const auto &value : files) {
        const auto entry = value.toObject();
        const QString path = entry["path"].toString();
        if (entry["existed"].toBool()) {
            if (!writeFile(path, QByteArray::fromBase64(entry["original"].toString().toLatin1()), error)) return false;
        } else if (QFileInfo::exists(path) && !QFile::remove(path)) {
            *error = tr("Cannot remove %1").arg(path); return false;
        }
    }
    return true;
}

bool Backend::undo() {
    if (!canUndo()) return fail(tr("There is no saved change to undo."));
    const auto manifest = QJsonDocument::fromJson(read(m_lastBackup + "/manifest.json")).object();
    QString error;
    if (!restore(m_lastBackup, &error)) return fail(error);
    m_selected = manifest["theme"].toString("Adwaita");
    m_size = manifest["size"].toInt(24);
    m_active = m_selected;
    m_activeSize = m_size;
    // Keep the backup marker on runtime failure, so the user can locate recovery data.
    if (!applyRuntime(&error, manifest["runtimeTheme"].toString(), manifest["runtimeSize"].toInt()))
        return fail(error + tr(" Backup: %1").arg(m_lastBackup));
    QFile::remove(state() + "/cursora/last-backup");
    m_lastBackup.clear();
    m_failed = false;
    m_message = tr("Previous cursor settings restored.");
    emit changed();
    return true;
}

void Backend::discard() { m_selected = m_active; m_size = m_activeSize; emit changed(); }
void Backend::openThemesDirectory() {
    if (!QDir().mkpath(themesDirectory()) || !QDesktopServices::openUrl(QUrl::fromLocalFile(themesDirectory())))
        fail(tr("Cannot open the themes folder: %1").arg(themesDirectory()));
}
