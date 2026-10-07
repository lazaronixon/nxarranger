#pragma once

#include <QColor>
#include <QString>

// One arranger pad (e.g. "Variation 2") and the audio range assigned to it.
struct Section
{
    QString name;   // display and file name, e.g. "Intro 1"
    QString group;  // pad family, e.g. "Intro"
    QColor color;
    qint64 startFrame = -1;
    qint64 endFrame = -1;

    bool isSet() const { return startFrame >= 0 && endFrame > startFrame; }
    void clear() { startFrame = endFrame = -1; }
};
