// Original terminal-inspired cursor artwork, distributed under Cursora's MIT license.
#include "cursorimages.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <cstring>
#include <filesystem>
#include <X11/Xcursor/Xcursor.h>

namespace {
using Kind = QByteArray;
constexpr int BaseSize = 24;
constexpr QColor Ink(15, 15, 15);
constexpr QColor Paper(247, 247, 247);

void line(QPainterPath &path, std::initializer_list<QPointF> points) {
    auto point = points.begin();
    path.moveTo(*point++);
    for (; point != points.end(); ++point) path.lineTo(*point);
}

QPainterPath arrow() {
    QPainterPath path;
    line(path, {{4, 4}, {20, 13}, {12, 14}, {16, 20},
                {13, 21}, {9, 16}, {4, 19}, {4, 4}});
    path.closeSubpath();
    return path;
}

void drawGlyph(QPainter &painter, const QPainterPath &path, bool filled = false) {
    const auto pen = [](QColor color, qreal width) {
        return QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    };
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pen(Ink, filled ? 4.4 : 4.6));
    painter.drawPath(path);
    painter.setPen(pen(Paper, filled ? 2.8 : 3.2));
    painter.drawPath(path);
    painter.setPen(filled ? QPen(Qt::NoPen) : pen(Ink, 1.2));
    if (filled) painter.setBrush(Ink);
    painter.drawPath(path);
}

QPainterPath resizeGlyph(int angle, bool both) {
    QPainterPath path;
    line(path, {{-7, 0}, {7, 0}});
    line(path, {{3, -4}, {7, 0}, {3, 4}});
    if (both) line(path, {{-3, -4}, {-7, 0}, {-3, 4}});
    QTransform transform;
    transform.translate(12, 12);
    transform.rotate(angle);
    return transform.map(path);
}

QPoint logicalHotspot(const Kind &kind) {
    if (kind == "left_ptr" || kind == "context-menu" || kind == "copy" ||
        kind == "alias" || kind == "help" || kind == "progress") return {4, 4};
    if (kind == "hand2") return {9, 3};
    return {12, 12};
}

QImage render(const Kind &kind, int size, int frame) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = qreal(size) / BaseSize;
    painter.scale(scale, scale);
    QPainterPath path;
    if (kind == "left_ptr") {
        drawGlyph(painter, arrow(), true);
        painter.fillRect(QRectF(6, 9, 2, 2), Paper);
    } else if (kind == "hand2" || kind == "grab" || kind == "grabbing") {
        if (kind == "hand2") {
            line(path, {{9, 3}, {11, 3}, {11, 9}, {18, 9}, {20, 11}, {20, 16},
                        {16, 21}, {12, 21}, {5, 15}, {5, 12}, {9, 14}, {9, 3}});
        } else {
            const int top = kind == "grab" ? 5 : 8;
            line(path, {{5, 11}, {7, 11}, {7, qreal(top)}, {9, qreal(top)},
                        {9, 9}, {11, 9}, {11, qreal(top - 1)}, {13, qreal(top - 1)},
                        {13, 9}, {15, 9}, {15, qreal(top)}, {17, qreal(top)},
                        {17, 10}, {20, 10}, {20, 16}, {17, 21}, {10, 21}, {5, 15}, {5, 11}});
        }
        path.closeSubpath();
        drawGlyph(painter, path, true);
    } else if (kind == "xterm" || kind == "vertical-text") {
        line(path, {{8, 4}, {16, 4}});
        line(path, {{12, 4}, {12, 20}});
        line(path, {{8, 20}, {16, 20}});
        if (kind == "vertical-text") {
            QTransform transform;
            transform.translate(24, 0);
            transform.rotate(90);
            path = transform.map(path);
        }
        drawGlyph(painter, path);
    } else if (kind == "crosshair" || kind == "cell" || kind == "move") {
        line(path, {{12, 4}, {12, 20}});
        line(path, {{4, 12}, {20, 12}});
        if (kind == "cell") path.addRect(QRectF(8, 8, 8, 8));
        if (kind == "move") {
            line(path, {{9, 7}, {12, 4}, {15, 7}});
            line(path, {{9, 17}, {12, 20}, {15, 17}});
            line(path, {{7, 9}, {4, 12}, {7, 15}});
            line(path, {{17, 9}, {20, 12}, {17, 15}});
        }
        drawGlyph(painter, path);
    } else if (kind == "watch" || kind == "progress") {
        if (kind == "progress") {
            painter.save();
            painter.scale(.66, .66);
            drawGlyph(painter, arrow(), true);
            painter.restore();
            painter.translate(12, 12);
            painter.scale(.45, .45);
        }
        path.addRect(QRectF(4, 5, 16, 14));
        drawGlyph(painter, path, true);
        painter.fillRect(QRectF(7 + 3 * (frame % 4), 11, 2, 2), Paper);
    } else if (kind == "not-allowed" || kind == "zoom-in" || kind == "zoom-out") {
        if (kind == "not-allowed") {
            path.addRect(QRectF(5, 5, 14, 14));
            line(path, {{6, 18}, {18, 6}});
        } else {
            path.addRect(QRectF(4, 4, 11, 11));
            line(path, {{15, 15}, {20, 20}});
            line(path, {{7, 9.5}, {12, 9.5}});
            if (kind == "zoom-in") line(path, {{9.5, 7}, {9.5, 12}});
        }
        drawGlyph(painter, path);
    } else if (kind.endsWith("-resize")) {
        const Kind direction = kind.chopped(7);
        const QMap<Kind, int> angles{
            {"e", 0}, {"se", 45}, {"s", 90}, {"sw", 135}, {"w", 180},
            {"nw", 225}, {"n", 270}, {"ne", 315}, {"ew", 0},
            {"ns", 90}, {"nwse", 45}, {"nesw", 135}
        };
        drawGlyph(painter, resizeGlyph(angles.value(direction), direction.size() > 1));
    } else {
        painter.save();
        painter.scale(.75, .75);
        drawGlyph(painter, arrow(), true);
        painter.restore();
        path.addRect(QRectF(13, 13, 8, 8));
        drawGlyph(painter, path, true);
        QPainterPath badge;
        if (kind == "copy") {
            line(badge, {{15, 17}, {19, 17}});
            line(badge, {{17, 15}, {17, 19}});
        } else if (kind == "alias") {
            line(badge, {{15, 18}, {19, 18}, {19, 15}});
            line(badge, {{16, 15}, {19, 15}});
        } else if (kind == "help") {
            line(badge, {{15, 15}, {19, 15}, {19, 17}, {17, 17}, {17, 18}});
            line(badge, {{17, 19}, {17, 20}});
        } else {
            line(badge, {{15, 16}, {19, 16}});
            line(badge, {{15, 18}, {19, 18}});
        }
        painter.setPen(QPen(Paper, 1));
        painter.drawPath(badge);
    }
    return image;
}

bool writeCursor(const QString &filename, const Kind &kind) {
    // Rasterize each resolution directly from the artwork. Keep a matching
    // bitmap for 1x, 2x, 3x and 4x output scales.
    constexpr int sizes[]{24, 48, 72, 96};
    const int frames = kind == "watch" || kind == "progress" ? 4 : 1;
    auto *images = XcursorImagesCreate(4 * frames);
    if (!images) return false;
    const QPoint hotspot = logicalHotspot(kind);
    for (int size : sizes) {
        for (int frame = 0; frame < frames; ++frame) {
            auto *image = XcursorImageCreate(size, size);
            if (!image) {
                XcursorImagesDestroy(images);
                return false;
            }
            image->size = size;
            const qreal artScale = kind == "progress" ? .66 :
                kind == "context-menu" || kind == "copy" || kind == "alias" || kind == "help" ? .75 : 1;
            image->xhot = qRound(hotspot.x() * artScale * size / BaseSize);
            image->yhot = qRound(hotspot.y() * artScale * size / BaseSize);
            image->delay = frames > 1 ? 120 : 0;
            const QImage pixels = render(kind, size, frame);
            std::memcpy(image->pixels, pixels.constBits(), pixels.sizeInBytes());
            images->images[images->nimage++] = image;
        }
    }
    const bool ok = XcursorFilenameSaveImages(QFile::encodeName(filename).constData(), images);
    XcursorImagesDestroy(images);
    return ok;
}
} // namespace

bool createOmarchyCursorPack(const QString &directory, QString *error) {
    if (directory.isEmpty()) { *error = "Missing cursor pack output directory"; return false; }
    const QString cursors = directory + "/cursors";
    if (!QDir().mkpath(cursors)) { *error = "Cannot create " + cursors; return false; }
    QFile index(directory + "/index.theme");
    const QByteArray metadata = "[Icon Theme]\nName=Omarchy Terminal\n"
        "Comment=Original monochrome terminal-style cursor pack by Cursora\nX-Cursora-DefaultSize=24\n";
    if (!index.open(QIODevice::WriteOnly) || index.write(metadata) != metadata.size()) {
        *error = "Cannot write " + index.fileName(); return false;
    }
    index.close();
    const QList<Kind> shapes{
        "left_ptr", "hand2", "grab", "grabbing", "xterm", "vertical-text", "crosshair", "cell",
        "move", "watch", "progress", "not-allowed", "zoom-in", "zoom-out", "e-resize", "se-resize",
        "s-resize", "sw-resize", "w-resize", "nw-resize", "n-resize", "ne-resize", "ew-resize",
        "ns-resize", "nwse-resize", "nesw-resize", "context-menu", "copy", "alias", "help"
    };
    for (const auto &shape : shapes) {
        const QString path = cursors + "/" + QString::fromLatin1(shape);
        if (!writeCursor(path, shape)) { *error = "Cannot write " + path; return false; }
    }
    const QMap<Kind, Kind> aliases{
        {"default", "left_ptr"}, {"arrow", "left_ptr"}, {"top_left_arrow", "left_ptr"},
        {"right_ptr", "left_ptr"}, {"pointer", "hand2"}, {"hand1", "hand2"}, {"hand", "hand2"},
        {"text", "xterm"}, {"ibeam", "xterm"}, {"cross", "crosshair"}, {"tcross", "crosshair"},
        {"plus", "cell"}, {"all-scroll", "move"}, {"fleur", "move"}, {"size_all", "move"},
        {"wait", "watch"}, {"left_ptr_watch", "progress"}, {"half-busy", "progress"},
        {"no-drop", "not-allowed"}, {"crossed_circle", "not-allowed"}, {"forbidden", "not-allowed"},
        {"left_side", "w-resize"}, {"right_side", "e-resize"}, {"top_side", "n-resize"},
        {"bottom_side", "s-resize"}, {"top_left_corner", "nw-resize"}, {"top_right_corner", "ne-resize"},
        {"bottom_left_corner", "sw-resize"}, {"bottom_right_corner", "se-resize"},
        {"size_hor", "ew-resize"}, {"h_double_arrow", "ew-resize"}, {"sb_h_double_arrow", "ew-resize"},
        {"size_ver", "ns-resize"}, {"v_double_arrow", "ns-resize"}, {"sb_v_double_arrow", "ns-resize"},
        {"size_fdiag", "nwse-resize"}, {"bd_double_arrow", "nwse-resize"},
        {"size_bdiag", "nesw-resize"}, {"fd_double_arrow", "nesw-resize"},
        {"col-resize", "ew-resize"}, {"row-resize", "ns-resize"}, {"dnd-copy", "copy"},
        {"dnd-link", "alias"}, {"dnd-move", "move"}, {"dnd-none", "not-allowed"},
        {"dnd-ask", "help"}, {"question_arrow", "help"}, {"left_ptr_help", "help"},
        {"link", "alias"}, {"openhand", "grab"}, {"closedhand", "grabbing"}, {"X_cursor", "crosshair"},
        {"9d800788f1b08800ae810202380a0822", "hand2"}, {"08e8e1c95fe2fc01f976f1e063a24ccd", "progress"},
        {"028006030e0e7ebffc7f7070c0600140", "ew-resize"}, {"2870a09082c103050810ffdffffe0204", "ns-resize"},
        {"14fef782d02440884392942c11205230", "nwse-resize"}, {"c7088f0f3e6f80601f00000000000000", "nesw-resize"},
        {"03b6e0fcb3499374a867c041f52298f0", "not-allowed"}, {"1081e37283d90000800003c07f3ef6bf", "copy"},
        {"6407b0e94181790501fd1e167b474872", "copy"}, {"fcf1c3c7cd4491d801f1e1c78f100000", "alias"}
    };
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
        const QByteArray destination = QFile::encodeName(cursors + "/" + QString::fromLatin1(it.key()));
        std::error_code ec;
        std::filesystem::create_symlink(it.value().constData(), destination.constData(), ec);
        if (ec) { *error = "Cannot create cursor alias " + QString::fromLatin1(it.key()); return false; }
    }
    return true;
}
