#pragma once

#include "Arranger.h"

#include <QIODevice>
#include <QMutex>

class QAudioSink;

// Streams an Arranger to the default audio output. QAudioSink pulls frames
// through readData(), possibly from its own thread, so every Arranger call
// goes through m_mutex.
class ArrangerDevice : public QIODevice
{
    Q_OBJECT

public:
    explicit ArrangerDevice(QObject *parent = nullptr);
    ~ArrangerDevice() override;

    // `audio` must stay alive until shutdown().
    bool start(const AudioData *audio, const QVector<Arranger::Region> &regions, QString *error);
    void shutdown();
    bool isActive() const { return m_sink != nullptr; }

    void press(int index);
    void startStop();
    void stop();
    Arranger::State state() const;

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    mutable QMutex m_mutex;
    Arranger m_arranger;
    QAudioSink *m_sink = nullptr;
    int m_bytesPerFrame = 0;
};
