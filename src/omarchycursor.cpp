// Original terminal-inspired cursor artwork, distributed under Cursora's MIT license.
#include "cursorimages.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineF>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTransform>
#include <cstring>
#include <filesystem>
#include <X11/Xcursor/Xcursor.h>

namespace {
using Kind = QByteArray;
constexpr int BaseSize = 24;
constexpr int StraightResizePadding = 4;
constexpr qreal OutlineThickness = 1.35;
constexpr QColor Ink(15, 15, 15);
constexpr QColor Paper(247, 247, 247);

struct Palette {
    QColor fill;
    QColor outline;
};

void line(QPainterPath &path, std::initializer_list<QPointF> points) {
    auto point = points.begin();
    path.moveTo(*point++);
    for (; point != points.end(); ++point) path.lineTo(*point);
}

void drawArrow(QPainter &painter, const QPainterPath &outline, const Palette &palette) {
    // Inset the fill to keep the silhouette and tip in place.
    QPainterPathStroker border;
    border.setWidth(2 * OutlineThickness);
    border.setJoinStyle(Qt::MiterJoin);
    painter.fillPath(outline, palette.outline);
    painter.fillPath(outline.subtracted(border.createStroke(outline)), palette.fill);
}

void drawBadgeArrow(QPainter &painter, const Palette &palette, qreal scale) {
    QPainterPath outline;
    line(outline, {{2, 2}, {22, 14}, {13, 15}, {17, 21},
                   {12, 23}, {8, 17}, {2, 21}});
    outline.closeSubpath();
    drawArrow(painter, QTransform::fromScale(scale, scale).map(outline), palette);
}

void drawGlyph(QPainter &painter, const QPainterPath &path, const Palette &palette) {
    constexpr qreal strokeWidth = 2;
    const auto pen = [](QColor color, qreal width) {
        return QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    };
    painter.strokePath(path, pen(palette.outline, strokeWidth + 2 * OutlineThickness));
    painter.strokePath(path, pen(palette.fill, strokeWidth));
}

void drawOutlinedShape(QPainter &painter, const QPainterPath &path, const Palette &palette,
                       Qt::PenJoinStyle join = Qt::MiterJoin) {
    painter.strokePath(path, QPen(palette.outline, 2 * OutlineThickness, Qt::SolidLine, Qt::SquareCap, join));
    painter.fillPath(path, palette.fill);
}

void drawHand(QPainter &painter, const QPainterPath &shape, const QPainterPath &creases, const Palette &palette) {
    drawOutlinedShape(painter, shape, palette);
    painter.strokePath(creases, QPen(palette.outline, .9, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
}

QPainterPath linkGlyph() {
    QPainterPath path;
    // The three folded fingers step down toward the little finger as the hand tilts.
    line(path, {{4.85, 4.25}, {7.0, 2.85}, {11.4, 8.65},
                {11.65, 6.45}, {13.2, 6.05}, {14.35, 7.05}, {15.1, 9.5},
                {15.55, 7.45}, {17.1, 7.3}, {18.05, 8.4}, {18.8, 10.6},
                {19.25, 9.2}, {20.45, 9.4}, {21.4, 11.15}, {21.4, 16.85},
                {18.0, 21.1}, {12.35, 21.1}, {4.6, 17.95}, {3.25, 15.9},
                {4.65, 14.55}, {6.45, 14.55}, {9.35, 16.0}});
    path.closeSubpath();
    return path;
}

void drawGrabHand(QPainter &painter, bool gripping, const Palette &palette) {
    QPainterPath shape, creases;
    if (gripping) {
        line(shape, {{5.1, 10.15}, {5.5, 8.45}, {6.9, 7.45}, {8.55, 7.8}, {9.8, 8.9},
                     {9.2, 7.05}, {10.4, 6.15}, {12.1, 6.65}, {13.35, 8.6},
                     {13.3, 7.6}, {14.65, 7.4}, {16.15, 8.5}, {17.1, 10.25},
                     {17.55, 9.35}, {19, 9.8}, {20.55, 11.4}, {21.15, 14.8},
                     {20.35, 17.85}, {17.65, 21.05}, {11.9, 21.05}, {7.25, 19.3},
                     {4.75, 16.8}, {3.55, 14.25}, {4.2, 12.25}, {5.9, 11.65}, {8.45, 12.45}});
        line(creases, {{9.8, 8.9}, {10.55, 10.4}, {10.3, 12.0}});
        line(creases, {{13.35, 8.6}, {14.15, 10.55}, {14, 12.7}});
        line(creases, {{17.1, 10.25}, {17.85, 11.9}, {17.55, 13.55}});
        // The thumb folds across the curled fingers.
        line(creases, {{8.45, 12.45}, {10.9, 14.65}, {13.25, 14.8}, {14.45, 14.15}});
    } else {
        line(shape, {{4.55, 6.55}, {5.7, 5.5}, {7.15, 5.6}, {10.15, 10.25},
                     {7.4, 4.4}, {8.1, 3.25}, {9.65, 2.85}, {10.7, 3.7}, {13.05, 9.4},
                     {11.55, 5}, {12.6, 4.35}, {14.05, 4.65}, {16.05, 10.65},
                     {15.55, 7.45}, {16.9, 7.05}, {18.3, 7.75}, {20.45, 12.2},
                     {21, 15.6}, {20, 18.6}, {17.4, 21}, {11.65, 21}, {7.25, 18.95},
                     {3.7, 15.65}, {3.25, 14.1}, {4.4, 12.9}, {6.25, 13.45},
                     {8.95, 15.4}, {7.7, 12.4}});
        line(creases, {{10.15, 10.25}, {10.85, 11.4}, {11.35, 12.85}});
        line(creases, {{13.05, 9.4}, {13.85, 10.8}, {14.45, 12.65}});
        line(creases, {{16.05, 10.65}, {16.85, 11.9}, {17.35, 13.3}});
    }
    shape.closeSubpath();
    drawHand(painter, shape, creases, palette);
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
        line(path, {{-11, -2.25}, {1.35, -2.25}, {1.35, -5.2}, {7.8, 0},
                    {1.35, 5.2}, {1.35, 2.25}, {-11, 2.25}});
        path.closeSubpath();
        // The corner bracket keeps the same angle relative to the arrow in every direction.
        QPainterPath bracket;
        line(bracket, {{9.85, -9.85}, {9.85, 9.85}, {-9.85, 9.85},
                       {-9.85, 7.25}, {7.25, 7.25}, {7.25, -9.85}});
        bracket.closeSubpath();
        QTransform corner;
        corner.translate(12, 12);
        corner.rotate(angle - 45);
        QPainterPath glyph = transform.map(path);
        glyph.addPath(corner.map(bracket));
        return glyph;
    }
    return transform.map(path);
}

QPoint logicalHotspot(const Kind &kind) {
    if (kind == "left_ptr" || kind == "context-menu" || kind == "copy" ||
        kind == "alias" || kind == "help" || kind == "progress") return {2, 2};
    if (kind == "hand2") return {6, 3};
    return {12, 12};
}

QImage render(const Kind &kind, int size, int frame, int padding, const Palette &palette) {
    const int extent = size + 2 * padding;
    QImage image(extent, extent, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = qreal(size) / BaseSize;
    painter.translate(padding, padding);
    painter.scale(scale, scale);
    QPainterPath path;
    if (kind == "left_ptr") {
        const QPointF tip{2, 2};
        const QPointF neck{9.9, 16.1};
        const QPointF axis = QLineF(tip, neck).unitVector().p2() - tip;
        const QPointF across{axis.y(), -axis.x()};
        const QPointF end = neck + axis * 5.35;
        const QPointF shoulder = neck + across * 2.6;
        line(path, {tip, {17.5, shoulder.y()}, shoulder, end + across * 4.2,
                    end - across * 4.2, neck - across * 2.6, {2, 21}});
        path.closeSubpath();
        drawArrow(painter, path, palette);
    } else if (kind == "hand2") {
        // Short creases make the bent fingers readable at the native size.
        QPainterPath creases;
        line(creases, {{11.65, 8.8}, {12.55, 9.7}, {13.15, 11.65}});
        line(creases, {{15.3, 9.6}, {16.0, 10.45}, {16.45, 12.35}});
        line(creases, {{18.9, 10.65}, {19.45, 11.5}, {19.65, 13.1}});
        drawHand(painter, linkGlyph(), creases, palette);
    } else if (kind.endsWith("-resize")) {
        drawOutlinedShape(painter, resizeGlyph(kind.chopped(7)), palette, Qt::RoundJoin);
    } else if (kind == "grab" || kind == "grabbing") {
        drawGrabHand(painter, kind == "grabbing", palette);
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
        drawGlyph(painter, path, palette);
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
        drawGlyph(painter, path, palette);
    } else if (kind == "watch" || kind == "progress") {
        QTransform badge;
        if (kind == "progress") {
            drawBadgeArrow(painter, palette, .66);
            badge.translate(12, 12);
            badge.scale(.45, .45);
        }
        path.addRect(QRectF(4, 5, 16, 14));
        drawOutlinedShape(painter, badge.map(path), palette);
        painter.setWorldTransform(badge, true);
        QPainterPath prompt;
        line(prompt, {{7, 9}, {10, 12}, {7, 15}});
        painter.setPen(QPen(palette.outline, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawPath(prompt);
        painter.fillRect(QRectF(11 + 2 * frame, 14, 2, 2), palette.outline);
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
        drawGlyph(painter, path, palette);
    } else {
        drawBadgeArrow(painter, palette, .75);
        path.addRect(QRectF(13, 13, 8, 8));
        drawOutlinedShape(painter, path, palette);
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
        painter.setPen(QPen(palette.outline, 1));
        painter.drawPath(badge);
    }
    painter.end();
    return image;
}

bool writeCursor(const QString &filename, const Kind &kind, const Palette &palette) {
    // Rasterize each resolution directly from the artwork. Keep a matching
    // bitmap for 1x, 2x, 3x and 4x output scales.
    constexpr int sizes[]{24, 48, 72, 96};
    const int frames = kind == "watch" || kind == "progress" ? 4 : 1;
    auto *images = XcursorImagesCreate(4 * frames);
    if (!images) return false;
    const QPoint hotspot = logicalHotspot(kind);
    // The rotated corner needs extra canvas. Keep the nominal size and scale unchanged.
    const bool straightResize = kind.endsWith("-resize") && kind.chopped(7).size() == 1;
    for (int size : sizes) {
        const int padding = straightResize ? StraightResizePadding * size / BaseSize : 0;
        for (int frame = 0; frame < frames; ++frame) {
            const int extent = size + 2 * padding;
            auto *image = XcursorImageCreate(extent, extent);
            if (!image) {
                XcursorImagesDestroy(images);
                return false;
            }
            image->size = size;
            const qreal artScale = kind == "progress" ? .66 :
                kind == "context-menu" || kind == "copy" || kind == "alias" || kind == "help" ? .75 : 1;
            image->xhot = padding + qRound(hotspot.x() * artScale * size / BaseSize);
            image->yhot = padding + qRound(hotspot.y() * artScale * size / BaseSize);
            image->delay = frames > 1 ? 120 : 0;
            const QImage pixels = render(kind, size, frame, padding, palette);
            std::memcpy(image->pixels, pixels.constBits(), pixels.sizeInBytes());
            images->images[images->nimage++] = image;
        }
    }
    const bool ok = XcursorFilenameSaveImages(QFile::encodeName(filename).constData(), images);
    XcursorImagesDestroy(images);
    return ok;
}
} // namespace

bool createOmarchyCursorPack(const QString &directory, bool light, QString *error) {
    if (directory.isEmpty()) { *error = "Missing cursor pack output directory"; return false; }
    const QString cursors = directory + "/cursors";
    if (!QDir().mkpath(cursors)) { *error = "Cannot create " + cursors; return false; }
    QFile index(directory + "/index.theme");
    const QByteArray name = light ? "Omarchy Terminal Light" : "Omarchy Terminal";
    const QByteArray metadata = "[Icon Theme]\nName=" + name + "\n"
        "Comment=Original monochrome terminal-style cursor pack by Cursora\nX-Cursora-DefaultSize=24\n";
    if (!index.open(QIODevice::WriteOnly) || index.write(metadata) != metadata.size()) {
        *error = "Cannot write " + index.fileName(); return false;
    }
    index.close();
    const Palette palette = light ? Palette{Paper, Ink} : Palette{Ink, Paper};
    const QList<Kind> shapes{
        "left_ptr", "hand2", "grab", "grabbing", "xterm", "vertical-text", "crosshair", "cell",
        "move", "watch", "progress", "not-allowed", "zoom-in", "zoom-out", "e-resize", "se-resize",
        "s-resize", "sw-resize", "w-resize", "nw-resize", "n-resize", "ne-resize", "ew-resize",
        "ns-resize", "nwse-resize", "nesw-resize", "col-resize", "row-resize", "context-menu", "copy", "alias", "help"
    };
    for (const auto &shape : shapes) {
        const QString path = cursors + "/" + QString::fromLatin1(shape);
        if (!writeCursor(path, shape, palette)) { *error = "Cannot write " + path; return false; }
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
