#pragma once
#include <QQuickImageProvider>
#include <QString>

class CursorImages : public QQuickImageProvider {
public:
    CursorImages() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
bool generateThemes(const QString &source, const QString &destination, QString *error);
