#pragma once

#include "AudioData.h"

#include <QObject>

class QAudioDecoder;

// Decodes an audio file (MP3, etc.) to interleaved int16 PCM asynchronously.
class AudioLoader : public QObject
{
    Q_OBJECT

public:
    explicit AudioLoader(QObject *parent = nullptr);

    void load(const QString &path);
    void cancel();

signals:
    void progress(int percent);
    void finished(const AudioData &audio);
    void failed(const QString &message);

private:
    void onBufferReady();
    void onFinished();

    QAudioDecoder *m_decoder = nullptr;
    AudioData m_data;
};
