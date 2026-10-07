#include "ArrangerDevice.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QMutexLocker>

#include <algorithm>

namespace {

// Small enough that fills and breaks feel immediate (~45 ms at 44.1 kHz).
constexpr int kBufferFrames = 2048;

} // namespace

ArrangerDevice::ArrangerDevice(QObject *parent)
    : QIODevice(parent)
{
}

ArrangerDevice::~ArrangerDevice()
{
    shutdown();
}

bool ArrangerDevice::start(const AudioData *audio, const QVector<Arranger::Region> &regions, QString *error)
{
    shutdown();
    if (!audio || audio->isEmpty()) {
        if (error)
            *error = tr("No song loaded");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(audio->sampleRate);
    format.setChannelCount(audio->channels);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull() || !output.isFormatSupported(format)) {
        if (error)
            *error = tr("The audio output does not support %1 Hz, %2 channel 16-bit audio")
                         .arg(audio->sampleRate)
                         .arg(audio->channels);
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_arranger.setAudio(audio);
        m_arranger.setRegions(regions);
        m_arranger.setCrossfadeFrames(audio->sampleRate / 300); // ~3 ms
    }
    m_bytesPerFrame = audio->channels * int(sizeof(int16_t));

    open(QIODevice::ReadOnly);
    m_sink = new QAudioSink(output, format, this);
    m_sink->setBufferSize(kBufferFrames * m_bytesPerFrame);
    m_sink->start(this);
    if (m_sink->error() != QAudio::NoError) {
        if (error)
            *error = tr("Could not start audio output");
        shutdown();
        return false;
    }
    return true;
}

void ArrangerDevice::shutdown()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    if (isOpen())
        close();
    QMutexLocker lock(&m_mutex);
    m_arranger.setAudio(nullptr);
}

void ArrangerDevice::press(int index)
{
    QMutexLocker lock(&m_mutex);
    m_arranger.press(index);
}

void ArrangerDevice::startStop()
{
    QMutexLocker lock(&m_mutex);
    m_arranger.startStop();
}

void ArrangerDevice::stop()
{
    QMutexLocker lock(&m_mutex);
    m_arranger.stop();
}

Arranger::State ArrangerDevice::state() const
{
    QMutexLocker lock(&m_mutex);
    return m_arranger.state();
}

// The stream never ends (silence while stopped), so always offer a buffer's worth.
qint64 ArrangerDevice::bytesAvailable() const
{
    return qint64(kBufferFrames) * m_bytesPerFrame + QIODevice::bytesAvailable();
}

qint64 ArrangerDevice::readData(char *data, qint64 maxSize)
{
    if (m_bytesPerFrame <= 0)
        return 0;
    const qint64 frames = maxSize / m_bytesPerFrame;
    if (frames <= 0)
        return 0;
    std::fill(data, data + frames * m_bytesPerFrame, 0);
    QMutexLocker lock(&m_mutex);
    m_arranger.render(reinterpret_cast<int16_t *>(data), frames);
    return frames * m_bytesPerFrame;
}
