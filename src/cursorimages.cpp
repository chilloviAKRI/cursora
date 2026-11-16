#include "cursorimages.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <X11/Xcursor/Xcursor.h>

QImage CursorImages::requestImage(const QString &id, QSize *size, const QSize &) {
    // The provider receives an absolute cursor file, avoiding global theme state.
    const QString decoded = QUrl::fromPercentEncoding(id.toUtf8());
    const int divider = decoded.lastIndexOf('|');
    if (divider < 0) return {};
    const QString path = decoded.left(divider);
    const int requested = qBound(16, decoded.mid(divider + 1).toInt(), 128);
    XcursorImage *cursor = XcursorFilenameLoadImage(QFile::encodeName(path).constData(), requested);
    if (!cursor) return {};
    QImage image(reinterpret_cast<uchar *>(cursor->pixels), cursor->width, cursor->height,
                 QImage::Format_ARGB32_Premultiplied);
    QImage result = image.copy();
    XcursorImageDestroy(cursor);
    if (size) *size = result.size();
    return result;
}

bool monochromeThemes(const QString &directory, QString *error) {
    const auto themes = QDir(directory).entryInfoList({"Cursora-*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (themes.isEmpty()) { *error = "No cursor packs found in " + directory; return false; }
    for (const auto &theme : themes) {
        const auto files = QDir(theme.absoluteFilePath() + "/cursors").entryInfoList(QDir::Files);
        for (const auto &file : files) {
            if (file.isSymLink()) continue;
            const QByteArray path = QFile::encodeName(file.absoluteFilePath());
            XcursorImages *images = XcursorFilenameLoadAllImages(path.constData());
            if (!images) { *error = "Cannot read cursor: " + file.absoluteFilePath(); return false; }
            bool changed = false;
            for (int i = 0; i < images->nimage; ++i) {
                XcursorImage *image = images->images[i];
                for (unsigned int p = 0; p < image->width * image->height; ++p) {
                    const QRgb pixel = image->pixels[p];
                    if (!qAlpha(pixel)) continue;
                    const QRgb straight = qUnpremultiply(pixel);
                    if (qRed(straight) == qGreen(straight) && qRed(straight) == qBlue(straight)) continue;
                    const int gray = qGray(straight);
                    image->pixels[p] = qPremultiply(qRgba(gray, gray, gray, qAlpha(pixel)));
                    changed = true;
                }
            }
            // Keep hotspots, frame timing, sizes and aliases from the original pack.
            const bool ok = !changed || XcursorFilenameSaveImages(path.constData(), images);
            XcursorImagesDestroy(images);
            if (!ok) { *error = "Cannot save cursor: " + file.absoluteFilePath(); return false; }
        }
    }
    return true;
}
