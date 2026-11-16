#pragma once
#include <QObject>
#include <QVariantList>
#include <QMap>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY changed)
    Q_PROPERTY(QString active READ active NOTIFY changed)
    Q_PROPERTY(int cursorSize READ cursorSize WRITE setCursorSize NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(QString themesDirectory READ themesDirectory CONSTANT)
public:
    explicit Backend(const QString &home = {}, const QStringList &roots = {}, bool runtime = true);
    QVariantList themes() const { return m_themes; }
    QString selected() const { return m_selected; }
    QString active() const { return m_active; }
    int cursorSize() const { return m_size; }
    QString message() const { return m_message; }
    bool failed() const { return m_failed; }
    bool dirty() const { return m_selected != m_active || m_size != m_activeSize; }
    bool canUndo() const;
    QString themesDirectory() const;
    void setSelected(const QString &value);
    void setCursorSize(int value);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE bool apply();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE void discard();
    Q_INVOKABLE QString preview(const QString &id, const QString &shape, int size) const;
    Q_INVOKABLE void openThemesDirectory();
    static QByteArray iniValue(QByteArray data, const QString &section, const QString &key, const QString &value);
    static QByteArray managedBlock(QByteArray data, const QByteArray &content);
    QMap<QString, QByteArray> plannedFiles() const;
    QString lastBackup() const { return m_lastBackup; }
signals:
    void changed();
private:
    QString config() const;
    QString state() const;
    QString run(const QString &program, const QStringList &args, bool *ok = nullptr) const;
    bool writeFile(const QString &path, const QByteArray &data, QString *error) const;
    bool restore(const QString &directory, QString *error, bool verify = true);
    bool applyRuntime(QString *error, const QString &overrideTheme = {}, int overrideSize = 0);
    bool fail(const QString &message);
    QString m_home, m_config, m_state, m_data;
    QStringList m_roots;
    bool m_runtime;
    QVariantList m_themes;
    QString m_selected, m_active, m_message, m_lastBackup;
    int m_size = 24, m_activeSize = 24;
    bool m_failed = false;
};
