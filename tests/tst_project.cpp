#include "Project.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool touch(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}

} // namespace

class TestProject : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void songPathFollowsMovedFolder();
    void rejectsForeignJson();
};

void TestProject::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString song = dir.filePath("song.mp3");
    QVERIFY(touch(song));

    Project::Data in;
    in.songPath = song;
    in.sampleRate = 48000;
    in.regions = {{"Intro 1", 0, 96000}, {"Break", 480000, 576000}};

    QString error;
    const QString path = dir.filePath("set.nxa");
    QVERIFY2(Project::save(path, in, &error), qPrintable(error));

    Project::Data out;
    QVERIFY2(Project::load(path, &out, &error), qPrintable(error));
    QCOMPARE(out.songPath, QDir::cleanPath(song));
    QCOMPARE(out.sampleRate, 48000);
    QCOMPARE(out.regions.size(), 2);
    QCOMPARE(out.regions[1].name, QString("Break"));
    QCOMPARE(out.regions[1].startFrame, qint64(480000));
    QCOMPARE(out.regions[1].endFrame, qint64(576000));
}

void TestProject::songPathFollowsMovedFolder()
{
    QTemporaryDir a, b;
    QVERIFY(touch(a.filePath("song.mp3")));

    Project::Data in;
    in.songPath = a.filePath("song.mp3");
    QVERIFY(Project::save(a.filePath("set.nxa"), in));

    // Move project + song together to a new folder.
    QVERIFY(QFile::copy(a.filePath("set.nxa"), b.filePath("set.nxa")));
    QVERIFY(QFile::copy(a.filePath("song.mp3"), b.filePath("song.mp3")));

    Project::Data out;
    QVERIFY(Project::load(b.filePath("set.nxa"), &out));
    QCOMPARE(out.songPath, QDir::cleanPath(b.filePath("song.mp3")));
}

void TestProject::rejectsForeignJson()
{
    QTemporaryDir dir;
    QFile f(dir.filePath("x.nxa"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"hello\": 1}");
    f.close();

    Project::Data out;
    QString error;
    QVERIFY(!Project::load(f.fileName(), &out, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(TestProject)
#include "tst_project.moc"
