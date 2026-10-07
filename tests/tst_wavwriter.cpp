#include "WavWriter.h"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestWavWriter : public QObject
{
    Q_OBJECT

private slots:
    void writesHeaderAndSlice();
    void rejectsEmptyRange();
};

void TestWavWriter::writesHeaderAndSlice()
{
    AudioData audio;
    audio.sampleRate = 44100;
    audio.channels = 2;
    for (int i = 0; i < 10; ++i) {
        audio.samples.push_back(static_cast<int16_t>(i));
        audio.samples.push_back(static_cast<int16_t>(-i));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("slice.wav");
    QString error;
    QVERIFY2(WavWriter::write(path, audio, 2, 5, &error), qPrintable(error));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.size(), qint64(44 + 3 * 2 * 2));

    QDataStream in(&f);
    in.setByteOrder(QDataStream::LittleEndian);
    char tag[4];
    quint32 u32;
    quint16 u16;

    in.readRawData(tag, 4); QCOMPARE(QByteArray(tag, 4), QByteArray("RIFF"));
    in >> u32;              QCOMPARE(u32, quint32(36 + 12));
    in.readRawData(tag, 4); QCOMPARE(QByteArray(tag, 4), QByteArray("WAVE"));
    in.readRawData(tag, 4); QCOMPARE(QByteArray(tag, 4), QByteArray("fmt "));
    in >> u32;              QCOMPARE(u32, quint32(16));
    in >> u16;              QCOMPARE(u16, quint16(1));      // PCM
    in >> u16;              QCOMPARE(u16, quint16(2));      // channels
    in >> u32;              QCOMPARE(u32, quint32(44100));  // sample rate
    in >> u32;              QCOMPARE(u32, quint32(44100 * 4));
    in >> u16;              QCOMPARE(u16, quint16(4));      // block align
    in >> u16;              QCOMPARE(u16, quint16(16));     // bits
    in.readRawData(tag, 4); QCOMPARE(QByteArray(tag, 4), QByteArray("data"));
    in >> u32;              QCOMPARE(u32, quint32(12));

    for (int frame = 2; frame < 5; ++frame) {
        qint16 l, r;
        in >> l >> r;
        QCOMPARE(l, qint16(frame));
        QCOMPARE(r, qint16(-frame));
    }
}

void TestWavWriter::rejectsEmptyRange()
{
    AudioData audio;
    audio.sampleRate = 44100;
    audio.channels = 1;
    audio.samples = {1, 2, 3};

    QTemporaryDir dir;
    QString error;
    QVERIFY(!WavWriter::write(dir.filePath("x.wav"), audio, 2, 2, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(TestWavWriter)
#include "tst_wavwriter.moc"
