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

bool generateThemes(const QString &source, const QString &destination, QString *error) {
    const QList<QPair<QString, QString>> palettes = {
        {"Ocean", "#68aaff"}, {"Mint", "#73dfb1"}, {"Violet", "#bc9bff"},
        {"Amber", "#ffbd66"}, {"Rose", "#ff91b3"}, {"Ice", "#c5f1ff"}
    };
    if (!QFileInfo::exists(source + "/cursors/default")) {
        *error = "Adwaita cursors are missing: " + source;
        return false;
    }
    for (const auto &palette : palettes) {
        const QString root = destination + "/Cursora-" + palette.first;
        if (!QDir().mkpath(root + "/cursors")) { *error = "Cannot create " + root; return false; }
        QFile index(root + "/index.theme");
        if (!index.open(QIODevice::WriteOnly)) { *error = index.errorString(); return false; }
        index.write(QString("[Icon Theme]\nName=Cursora %1\nComment=Adwaita recolored by Cursora\nInherits=Adwaita\n").arg(palette.first).toUtf8());
        index.close();
        const QColor tint(palette.second);
        const auto entries = QDir(source + "/cursors").entryInfoList(QDir::Files, QDir::Name);
        for (const auto &entry : entries) {
            const QString out = root + "/cursors/" + entry.fileName();
            if (entry.isSymLink()) {
                QFile original(entry.absoluteFilePath());
                const QString target = QFileInfo(original.symLinkTarget()).fileName();
                QFile::remove(out);
                // Relative symlinks remain relocatable inside the package.
                if (!QFile::link(target, out)) { *error = "Cannot link " + out; return false; }
                continue;
            }
            XcursorImages *images = XcursorFilenameLoadAllImages(QFile::encodeName(entry.absoluteFilePath()).constData());
            if (!images) { *error = "Cannot read " + entry.absoluteFilePath(); return false; }
            for (int i = 0; i < images->nimage; ++i) {
                XcursorImage *image = images->images[i];
                for (unsigned int p = 0; p < image->width * image->height; ++p) {
                    const QRgb pixel = image->pixels[p];
                    const int alpha = qAlpha(pixel);
                    if (!alpha) continue;
                    const QRgb rgb = qUnpremultiply(pixel);
                    // Replace dark neutral fills; retain white outlines and colored status marks.
                    const int hi = qMax(qRed(rgb), qMax(qGreen(rgb), qBlue(rgb)));
                    const int lo = qMin(qRed(rgb), qMin(qGreen(rgb), qBlue(rgb)));
                    if (hi < 160 && hi - lo < 24) {
                        const qreal shade = 0.85 + hi / 1066.0;
                        image->pixels[p] = qPremultiply(qRgba(qRound(tint.red() * shade),
                            qRound(tint.green() * shade), qRound(tint.blue() * shade), alpha));
                    }
                }
            }
            const bool ok = XcursorFilenameSaveImages(QFile::encodeName(out).constData(), images);
            XcursorImagesDestroy(images);
            if (!ok) { *error = "Cannot write " + out; return false; }
        }
        QFile notice(root + "/COPYING");
        if (!notice.open(QIODevice::WriteOnly)) { *error = notice.errorString(); return false; }
        notice.write("Based on Adwaita cursors by the GNOME Project, https://gitlab.gnome.org/GNOME/adwaita-icon-theme\n"
                     "Color modifications by Cursora contributors, 2026. Licensed under CC BY-SA 3.0.\n"
                     "https://creativecommons.org/licenses/by-sa/3.0/\n");
        QFile license("/usr/share/licenses/spdx/CC-BY-SA-3.0.txt");
        if (!license.open(QIODevice::ReadOnly)) { *error = license.errorString(); return false; }
        notice.write(license.readAll());
    }
    return true;
}
