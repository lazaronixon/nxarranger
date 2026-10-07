#pragma once

#include "AudioData.h"

#include <QMainWindow>

class AudioLoader;
class PadPanel;
class QAction;
class QAudioOutput;
class QLabel;
class QMediaPlayer;
class QProgressBar;
class QScrollBar;
class QTimer;
class SectionModel;
class WaveformView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openFile(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void openDialog();
    void exportSections();
    void onAudioLoaded(const AudioData &audio);
    void onLoadFailed(const QString &message);

    void togglePlay();
    void stop();
    void toggleSection();
    void playSection();
    void onPlaybackTick();
    void updateStartStop();
    void seekToFrame(qint64 frame);

    void syncScrollBar();
    void updateActions();
    void updateRangeLabel();

    qint64 msToFrame(qint64 ms) const;
    qint64 frameToMs(qint64 frame) const;

    AudioData m_audio;
    QString m_path;

    SectionModel *m_model;
    WaveformView *m_waveform;
    PadPanel *m_pads;
    QScrollBar *m_scroll;
    AudioLoader *m_loader;
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOut;
    QTimer *m_tick;

    QLabel *m_fileLabel;
    QLabel *m_rangeLabel;
    QProgressBar *m_progress;

    QAction *m_openAct = nullptr;
    QAction *m_exportAct = nullptr;
    QAction *m_playAct = nullptr;
    QAction *m_stopAct = nullptr;
    QAction *m_playSectionAct = nullptr;
    QAction *m_loopAct = nullptr;
    QAction *m_fitAct = nullptr;
    QAction *m_clearAct = nullptr;

    // While previewing a section, playback stops (or loops) at this position.
    qint64 m_sectionStartMs = -1;
    qint64 m_sectionEndMs = -1;
};
