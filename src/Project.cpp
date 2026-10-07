#include "Project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Project {

namespace {

constexpr int kVersion = 1;

bool fail(QString *error, const QString &msg)
{
    if (error)
        *error = msg;
    return false;
}

} // namespace

bool save(const QString &path, const Data &data, QString *error)
{
    QJsonObject root;
    root["app"] = "NXArranger";
    root["version"] = kVersion;

    // Store the song relative to the project so a moved folder still opens,
    // with the absolute path as a fallback.
    if (!data.songPath.isEmpty()) {
        const QDir projectDir = QFileInfo(path).absoluteDir();
        root["song"] = projectDir.relativeFilePath(data.songPath);
        root["songAbsolute"] = QFileInfo(data.songPath).absoluteFilePath();
    }
    root["sampleRate"] = data.sampleRate;

    QJsonArray regions;
    for (const Region &r : data.regions) {
        QJsonObject o;
        o["name"] = r.name;
        o["start"] = r.startFrame;
        o["end"] = r.endFrame;
        regions.append(o);
    }
    root["regions"] = regions;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return fail(error, file.errorString());
    return true;
}

bool load(const QString &path, Data *data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, file.errorString());

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull())
        return fail(error, parseError.errorString());
    const QJsonObject root = doc.object();
    if (root["app"].toString() != "NXArranger")
        return fail(error, QStringLiteral("Not an NXArranger project"));
    if (root["version"].toInt() > kVersion)
        return fail(error, QStringLiteral("Project was saved by a newer version of NXArranger"));

    Data result;
    const QString relative = root["song"].toString();
    const QString absolute = root["songAbsolute"].toString();
    if (!relative.isEmpty()) {
        const QString resolved = QFileInfo(path).absoluteDir().absoluteFilePath(relative);
        result.songPath = QFileInfo::exists(resolved) || absolute.isEmpty() ? QDir::cleanPath(resolved) : absolute;
    }
    result.sampleRate = root["sampleRate"].toInt();

    for (const QJsonValue &v : root["regions"].toArray()) {
        const QJsonObject o = v.toObject();
        Region r;
        r.name = o["name"].toString();
        r.startFrame = o["start"].toInteger();
        r.endFrame = o["end"].toInteger();
        if (!r.name.isEmpty() && r.endFrame > r.startFrame && r.startFrame >= 0)
            result.regions.push_back(r);
    }

    *data = result;
    return true;
}

} // namespace Project
