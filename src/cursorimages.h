#pragma once
#include <QQuickImageProvider>
#include <QString>

class CursorImages : public QQuickImageProvider {
public:
    CursorImages() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
bool monochromeThemes(const QString &directory, QString *error);
int nativeCursorSize(const QString &path, int preferred = 24);
bool createOmarchyCursorPack(const QString &directory, QString *error);
