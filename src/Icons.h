#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace Icons {

// Loads :/icons/<name>.svg, replacing `currentColor` with `color`, so the
// monochrome icons follow the light/dark palette.
QIcon themed(const QString &name, const QColor &color);

} // namespace Icons
