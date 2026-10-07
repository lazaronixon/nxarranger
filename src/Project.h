#pragma once

#include <QString>
#include <QVector>

// On-disk project: the song plus each pad's region, stored as JSON (.nxa).
namespace Project {

struct Region
{
    QString name;
    qint64 startFrame = 0;
    qint64 endFrame = 0;
};

struct Data
{
    QString songPath;     // absolute path once loaded
    int sampleRate = 0;   // rate the frame positions refer to
    QVector<Region> regions;
};

inline const char *kFileFilter = "NXArranger projects (*.nxa)";
inline const char *kSuffix = "nxa";

bool save(const QString &path, const Data &data, QString *error = nullptr);
bool load(const QString &path, Data *data, QString *error = nullptr);

} // namespace Project
