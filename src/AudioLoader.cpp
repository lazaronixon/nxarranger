#include "AudioLoader.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {

int16_t floatToInt16(float v)
{
    v = std::clamp(v, -1.0f, 1.0f);
    return static_cast<int16_t>(std::lround(v * 32767.0f));
}

} // namespace

AudioLoader::AudioLoader(QObject *parent)
    : QObject(parent)
    , m_decoder(new QAudioDecoder(this))
{
    connect(m_decoder, &QAudioDecoder::bufferReady, this, &AudioLoader::onBufferReady);
    connect(m_decoder, &QAudioDecoder::finished, this, &AudioLoader::onFinished);
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) {
                const QString msg = m_decoder->errorString();
                m_decoder->stop();
                m_data = {};
                emit failed(msg.isEmpty() ? tr("Could not decode file") : msg);
            });
}

void AudioLoader::load(const QString &path)
{
    cancel();
    m_data = {};
    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
}

void AudioLoader::cancel()
{
    if (m_decoder->isDecoding())
        m_decoder->stop();
}

void AudioLoader::onBufferReady()
{
    const QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid())
        return;

    const QAudioFormat fmt = buffer.format();
    if (m_data.sampleRate == 0) {
        m_data.sampleRate = fmt.sampleRate();
        m_data.channels = fmt.channelCount();
    }
    // Ignore buffers whose layout changes mid-stream; decoders shouldn't do this.
    if (fmt.channelCount() != m_data.channels)
        return;

    const qsizetype count = buffer.sampleCount();
    m_data.samples.reserve(m_data.samples.size() + count);

    switch (fmt.sampleFormat()) {
    case QAudioFormat::UInt8: {
        const auto *p = buffer.constData<quint8>();
        for (qsizetype i = 0; i < count; ++i)
            m_data.samples.push_back(static_cast<int16_t>((int(p[i]) - 128) << 8));
        break;
    }
    case QAudioFormat::Int16: {
        const auto *p = buffer.constData<qint16>();
        m_data.samples.insert(m_data.samples.end(), p, p + count);
        break;
    }
    case QAudioFormat::Int32: {
        const auto *p = buffer.constData<qint32>();
        for (qsizetype i = 0; i < count; ++i)
            m_data.samples.push_back(static_cast<int16_t>(p[i] >> 16));
        break;
    }
    case QAudioFormat::Float: {
        const auto *p = buffer.constData<float>();
        for (qsizetype i = 0; i < count; ++i)
            m_data.samples.push_back(floatToInt16(p[i]));
        break;
    }
    default:
        break;
    }

    const qint64 duration = m_decoder->duration();
    if (duration > 0)
        emit progress(int(std::clamp<qint64>(m_decoder->position() * 100 / duration, 0, 100)));
}

void AudioLoader::onFinished()
{
    if (m_data.isEmpty()) {
        emit failed(tr("File contains no audio"));
        return;
    }
    emit progress(100);
    emit finished(m_data);
    m_data = {};
}
