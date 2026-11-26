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

void drawArrow(QPainter &painter) {
    // The nested shapes give the pointer one outline without a shadow.
    QPainterPath outline, body;
    line(outline, {{2, 2}, {22, 14}, {13, 15}, {17, 21},
                   {12, 23}, {8, 17}, {2, 21}, {2, 2}});
    line(body, {{4, 5}, {18, 13.25}, {10.5, 14}, {14.75, 20.25},
                {12.75, 21.25}, {8.75, 14.75}, {4, 18}, {4, 5}});
    painter.fillPath(outline, Paper);
    painter.fillPath(body, Ink);
}

void drawGlyph(QPainter &painter, const QPainterPath &path, bool filled = false) {
    const auto pen = [](QColor color, qreal width) {
        return QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    };
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pen(Paper, 3.2));
    painter.drawPath(path);
    painter.setPen(filled ? QPen(Qt::NoPen) : pen(Ink, 1.2));
    if (filled) painter.setBrush(Ink);
    painter.drawPath(path);
}

void drawOutlinedShape(QPainter &painter, const QPainterPath &path, qreal outlineWidth,
                       Qt::PenJoinStyle join) {
    painter.strokePath(path, QPen(Paper, outlineWidth, Qt::SolidLine, Qt::SquareCap, join));
    painter.fillPath(path, Ink);
}

QPainterPath linkGlyph() {
    QPainterPath path;
    // Keep the tilted index finger and bent wrist, with straight facets
    // around the fingertip, knuckles and thumb to match the main pointer.
    line(path, {{4.85, 4.25}, {7.0, 2.85}, {11.7, 8.8}, {12.2, 7.4},
                {13.65, 7.9}, {15, 9.6}, {16.1, 8.75}, {17.05, 9.45},
                {18.2, 11.2}, {19.45, 10.8}, {21.4, 13.2}, {21.4, 17.0},
                {18.2, 21.1}, {14.15, 21.1}, {6.6, 17.0}, {5.95, 15.6},
                {7.5, 14.45}, {10.8, 16}});
    path.closeSubpath();
    return path;
}

QPainterPath resizeGlyph(const Kind &direction) {
    static const QMap<Kind, int> angles{
        {"e", 0}, {"se", 45}, {"s", 90}, {"sw", 135}, {"w", 180},
        {"nw", 225}, {"n", 270}, {"ne", 315}, {"ew", 0},
        {"ns", 90}, {"nwse", 45}, {"nesw", 135}, {"col", 0}, {"row", 90}
    };
    const int angle = angles.value(direction);
    const bool diagonal = angle % 90 != 0;
    QTransform transform;
    transform.translate(12, 12);
    transform.rotate(angle);
    QPainterPath path;
    if (direction == "col" || direction == "row") {
        line(path, {{3.6, -2.15}, {5.1, -2.15}, {5.1, -5.2}, {9.85, 0},
                    {5.1, 5.2}, {5.1, 2.15}, {3.6, 2.15}});
        path.closeSubpath();
        path.addPath(QTransform::fromScale(-1, 1).map(path));
        path.addRect(QRectF(-1.25, -9.85, 2.5, 19.7));
    } else if (direction.size() > 2 || direction == "ew" || direction == "ns") {
        // Diagonals need a longer axis to fill a similar amount of the square.
        const qreal tip = diagonal ? 11.9 : 9.85;
        const qreal shoulder = diagonal ? 5.5 : 4.35;
        line(path, {{-tip, 0}, {-shoulder, -5.25}, {-shoulder, -2.35},
                    {shoulder, -2.35}, {shoulder, -5.25}, {tip, 0},
                    {shoulder, 5.25}, {shoulder, 2.35}, {-shoulder, 2.35}, {-shoulder, 5.25}});
        path.closeSubpath();
    } else {
        const qreal tail = diagonal ? -11 : -9.85;
        const qreal tip = diagonal ? 7.8 : 5.5;
        const qreal shoulder = diagonal ? 1.35 : -.25;
        line(path, {{tail, -2.25}, {shoulder, -2.25}, {shoulder, -5.2}, {tip, 0},
                    {shoulder, 5.2}, {shoulder, 2.25}, {tail, 2.25}});
        path.closeSubpath();
        if (diagonal) {
            // A corner bracket reads as a window corner at a glance.
            QPainterPath bracket;
            line(bracket, {{9.85, -1.75}, {9.85, 9.85}, {-1.75, 9.85},
                           {-1.75, 7.25}, {7.25, 7.25}, {7.25, -1.75}});
            bracket.closeSubpath();
            QTransform corner;
            corner.translate(12, 12);
            corner.rotate(angle - 45);
            QPainterPath glyph = transform.map(path);
            glyph.addPath(corner.map(bracket));
            return glyph;
        }
        path.addRect(QRectF(7.75, -8.8, 2.1, 17.6));
    }
    return transform.map(path);
}

QPoint logicalHotspot(const Kind &kind) {
    if (kind == "left_ptr" || kind == "context-menu" || kind == "copy" ||
        kind == "alias" || kind == "help" || kind == "progress") return {2, 2};
    if (kind == "hand2") return {6, 3};
    return {12, 12};
}

QImage render(const Kind &kind, int size, int frame) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    const qreal scale = qreal(size) / BaseSize;
    painter.scale(scale, scale);
    QPainterPath path;
    if (kind == "left_ptr") {
        // Rasterize the original silhouette at this frame's actual size.
        // Smooth its edge coverage without resampling a smaller cursor.
        painter.setRenderHint(QPainter::Antialiasing);
        drawArrow(painter);
        painter.end();
        return image;
    } else if (kind == "hand2" || kind.endsWith("-resize")) {
        const bool link = kind == "hand2";
        painter.setRenderHint(QPainter::Antialiasing);
        drawOutlinedShape(painter, link ? linkGlyph() : resizeGlyph(kind.chopped(7)),
                          link ? 3.2 : 2.2, link ? Qt::MiterJoin : Qt::RoundJoin);
        if (link) {
            // Short creases make the bent fingers readable at the native size.
            QPainterPath creases;
            line(creases, {{12.15, 9.9}, {13.1, 10.55}, {13.65, 11.55}});
            line(creases, {{15.5, 11.1}, {16.3, 11.75}, {16.9, 12.7}});
            painter.strokePath(creases, QPen(Paper, .9, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        }
        painter.end();
        return image;
    } else if (kind == "grab" || kind == "grabbing") {
        const int top = kind == "grab" ? 5 : 8;
        line(path, {{5, 11}, {7, 11}, {7, qreal(top)}, {9, qreal(top)},
                    {9, 9}, {11, 9}, {11, qreal(top - 1)}, {13, qreal(top - 1)},
                    {13, 9}, {15, 9}, {15, qreal(top)}, {17, qreal(top)},
                    {17, 10}, {20, 10}, {20, 16}, {17, 21}, {10, 21}, {5, 15}, {5, 11}});
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
            drawArrow(painter);
            painter.restore();
            painter.translate(12, 12);
            painter.scale(.45, .45);
        }
        path.addRect(QRectF(4, 5, 16, 14));
        drawGlyph(painter, path, true);
        QPainterPath prompt;
        line(prompt, {{7, 9}, {10, 12}, {7, 15}});
        painter.setPen(QPen(Paper, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawPath(prompt);
        painter.fillRect(QRectF(11 + 2 * frame, 14, 2, 2), Paper);
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
    } else {
        painter.save();
        painter.scale(.75, .75);
        drawArrow(painter);
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
    painter.end();
    // Fractional transforms in the small badges can still leave partial
    // coverage. Keep these glyphs on their two-tone pixel grid.
    for (int y = 0; y < size; ++y) {
        auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            const QRgb pixel = row[x];
            const int alpha = qAlpha(pixel);
            row[x] = alpha < 128 ? 0 : qRed(pixel) * 2 >= alpha ? Paper.rgba() : Ink.rgba();
        }
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
        "ns-resize", "nwse-resize", "nesw-resize", "col-resize", "row-resize", "context-menu", "copy", "alias", "help"
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
        {"dnd-copy", "copy"},
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
