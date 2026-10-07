#pragma once

#include "AudioData.h"
#include "Project.h"

#include <QMainWindow>

class ArrangerDevice;
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
    ~MainWindow() override;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void refreshIcons();

    // File menu.
    void newProject();
    void openProjectDialog();
    void openProject(const QString &path);
    bool saveProject();
    bool saveProjectAs();
    bool writeProject(const QString &path);
    void importSongDialog();
    void importSong(const QString &path);
    void exportRegions();
    bool maybeSave();

    void resetSong();
    void loadSong(const QString &path);
    void onAudioLoaded(const AudioData &audio);
    void onLoadFailed(const QString &message);
    void applyPendingRegions();

    void togglePlay();
    void play();
    void stop();
    void toggleSection();
    void playSection();
    void onPlaybackTick();
    void updateStartStop();
    void seekToFrame(qint64 frame);

    // Perform (Pa3X-style) mode.
    bool performing() const;
    void setPerforming(bool on);
    void onPerformTick();

    void syncScrollBar();
    void updateActions();
    void updateRangeLabel();
    void updateTitle();

    qint64 msToFrame(qint64 ms) const;
    qint64 frameToMs(qint64 frame) const;

    AudioData m_audio;
    QString m_path;          // song file
    QString m_projectPath;   // .nxa file, empty until first save
    bool m_loading = false;
    bool m_trackChanges = true;

    // Regions from an opened project, applied once its song finishes decoding.
    QVector<Project::Region> m_pendingRegions;
    int m_pendingRate = 0;

    SectionModel *m_model;
    WaveformView *m_waveform;
    PadPanel *m_pads;
    QScrollBar *m_scroll;
    AudioLoader *m_loader;
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOut;
    QTimer *m_tick;
    ArrangerDevice *m_arranger = nullptr;
    QTimer *m_performTick = nullptr;

    QLabel *m_fileLabel;
    QLabel *m_rangeLabel;
    QProgressBar *m_progress;

    QAction *m_newAct = nullptr;
    QAction *m_openAct = nullptr;
    QAction *m_saveAct = nullptr;
    QAction *m_saveAsAct = nullptr;
    QAction *m_importAct = nullptr;
    QAction *m_exportAct = nullptr;
    QAction *m_playAct = nullptr;
    QAction *m_pauseAct = nullptr;
    QAction *m_playPauseAct = nullptr;
    QAction *m_stopAct = nullptr;
    QAction *m_goToStartAct = nullptr;
    QAction *m_performAct = nullptr;
    QAction *m_fitAct = nullptr;
    QAction *m_clearAct = nullptr;

    // While previewing a section, playback stops at this position.
    qint64 m_sectionStartMs = -1;
    qint64 m_sectionEndMs = -1;
    // Where Stop returns the playhead; -1 when no play pass is in progress.
    qint64 m_playOriginMs = -1;
};
