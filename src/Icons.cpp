#include "Icons.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace Icons {

QIcon themed(const QString &name, const QColor &color)
{
    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name().toLatin1());

    QSvgRenderer renderer(svg);
    QIcon icon;
    // Render at 1x and 2x so the icon stays crisp on Retina displays.
    for (int scale : {1, 2}) {
        const int size = 24 * scale;
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        renderer.render(&p);
        p.end();
        pixmap.setDevicePixelRatio(scale);
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace Icons
