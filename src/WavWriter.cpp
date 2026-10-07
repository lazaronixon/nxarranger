#include "WavWriter.h"

#include <QDataStream>
#include <QSaveFile>

#include <algorithm>

namespace WavWriter {

bool write(const QString &path, const AudioData &audio, int64_t startFrame, int64_t endFrame,
           QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error)
            *error = msg;
        return false;
    };

    if (audio.isEmpty())
        return fail(QStringLiteral("No audio loaded"));

    startFrame = std::clamp<int64_t>(startFrame, 0, audio.frames());
    endFrame = std::clamp<int64_t>(endFrame, 0, audio.frames());
    if (endFrame <= startFrame)
        return fail(QStringLiteral("Empty range"));

    const quint16 channels = static_cast<quint16>(audio.channels);
    const quint32 sampleRate = static_cast<quint32>(audio.sampleRate);
    const quint16 bitsPerSample = 16;
    const quint16 blockAlign = channels * bitsPerSample / 8;
    const quint32 byteRate = sampleRate * blockAlign;
    const int64_t dataBytes64 = (endFrame - startFrame) * blockAlign;
    if (dataBytes64 > 0xFFFFFFFFLL - 36)
        return fail(QStringLiteral("Range too large for WAV"));
    const quint32 dataBytes = static_cast<quint32>(dataBytes64);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataBytes);
    out.writeRawData("WAVE", 4);

    out.writeRawData("fmt ", 4);
    out << quint32(16) << quint16(1) << channels << sampleRate << byteRate << blockAlign
        << bitsPerSample;

    out.writeRawData("data", 4);
    out << dataBytes;

    const int16_t *begin = audio.samples.data() + startFrame * audio.channels;
    const int16_t *end = audio.samples.data() + endFrame * audio.channels;
    for (const int16_t *p = begin; p != end; ++p)
        out << qint16(*p);

    if (out.status() != QDataStream::Ok)
        return fail(QStringLiteral("Write error"));
    if (!file.commit())
        return fail(file.errorString());
    return true;
}

} // namespace WavWriter
