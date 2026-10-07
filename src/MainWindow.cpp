#include "MainWindow.h"

#include "AudioLoader.h"
#include "PadPanel.h"
#include "SectionModel.h"
#include "WavWriter.h"
#include "WaveformView.h"

#include <QAction>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {

QString formatSeconds(double s)
{
    const int m = int(s) / 60;
    return QStringLiteral("%1:%2").arg(m).arg(s - m * 60, 6, 'f', 3, QLatin1Char('0'));
}

bool isAudioFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "mp3" || suffix == "wav" || suffix == "m4a" || suffix == "flac" || suffix == "ogg";
}

bool isProjectFile(const QString &path)
{
    return QFileInfo(path).suffix().compare(Project::kSuffix, Qt::CaseInsensitive) == 0;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_model(new SectionModel(this))
    , m_loader(new AudioLoader(this))
    , m_player(new QMediaPlayer(this))
    , m_audioOut(new QAudioOutput(this))
    , m_tick(new QTimer(this))
{
    setWindowTitle("NXArranger");
    setAcceptDrops(true);
    resize(1200, 520);

    m_waveform = new WaveformView(m_model);
    m_scroll = new QScrollBar(Qt::Horizontal);
    m_pads = new PadPanel(m_model);

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_waveform, 1);
    layout->addWidget(m_scroll);
    layout->addWidget(m_pads);
    setCentralWidget(central);

    m_fileLabel = new QLabel(tr("No file"));
    m_rangeLabel = new QLabel;
    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(180);
    m_progress->setRange(0, 100);
    m_progress->hide();
    statusBar()->addWidget(m_fileLabel);
    statusBar()->addWidget(m_rangeLabel, 1);
    statusBar()->addPermanentWidget(m_progress);

    m_player->setAudioOutput(m_audioOut);
    m_tick->setInterval(20);

    createActions();

    connect(m_loader, &AudioLoader::progress, m_progress, &QProgressBar::setValue);
    connect(m_loader, &AudioLoader::finished, this, &MainWindow::onAudioLoaded);
    connect(m_loader, &AudioLoader::failed, this, &MainWindow::onLoadFailed);

    connect(m_waveform, &WaveformView::viewChanged, this, &MainWindow::syncScrollBar);
    connect(m_waveform, &WaveformView::seekRequested, this, &MainWindow::seekToFrame);
    connect(m_scroll, &QScrollBar::valueChanged, this, [this](int v) {
        if (v != m_waveform->viewStart())
            m_waveform->setViewStart(v);
    });

    connect(m_model, &SectionModel::sectionChanged, this, [this] {
        if (m_trackChanges)
            setWindowModified(true);
        updateActions();
        updateRangeLabel();
    });
    connect(m_model, &SectionModel::armedChanged, this, [this] {
        updateActions();
        updateRangeLabel();
    });

    connect(m_tick, &QTimer::timeout, this, &MainWindow::onPlaybackTick);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        const bool playing = state == QMediaPlayer::PlayingState;
        updateActions();
        if (playing)
            m_tick->start();
        else
            m_tick->stop();
        onPlaybackTick();
        updateStartStop();
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &msg) {
        statusBar()->showMessage(tr("Playback error: %1").arg(msg), 5000);
    });

    m_model->setArmed(0);
    updateActions();
    updateRangeLabel();
    updateTitle();
}

void MainWindow::createActions()
{
    m_newAct = new QAction(tr("New"), this);
    m_newAct->setShortcut(QKeySequence::New);
    connect(m_newAct, &QAction::triggered, this, &MainWindow::newProject);

    m_openAct = new QAction(tr("Open…"), this);
    m_openAct->setShortcut(QKeySequence::Open);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openProjectDialog);

    m_saveAct = new QAction(tr("Save"), this);
    m_saveAct->setShortcut(QKeySequence::Save);
    connect(m_saveAct, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveAsAct = new QAction(tr("Save As…"), this);
    m_saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAct, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_importAct = new QAction(tr("Import Song…"), this);
    m_importAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(m_importAct, &QAction::triggered, this, &MainWindow::importSongDialog);

    m_exportAct = new QAction(tr("Export Regions…"), this);
    m_exportAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_exportAct, &QAction::triggered, this, &MainWindow::exportRegions);

    m_playAct = new QAction(tr("Play"), this);
    connect(m_playAct, &QAction::triggered, this, &MainWindow::play);

    m_pauseAct = new QAction(tr("Pause"), this);
    connect(m_pauseAct, &QAction::triggered, m_player, &QMediaPlayer::pause);

    m_playPauseAct = new QAction(tr("Play/Pause"), this);
    m_playPauseAct->setShortcut(Qt::Key_Space);
    connect(m_playPauseAct, &QAction::triggered, this, &MainWindow::togglePlay);

    m_stopAct = new QAction(tr("Stop"), this);
    m_stopAct->setShortcut(Qt::Key_Escape);
    connect(m_stopAct, &QAction::triggered, this, &MainWindow::stop);

    m_playSectionAct = new QAction(tr("Start/Stop Section"), this);
    m_playSectionAct->setShortcut(Qt::Key_Return);
    connect(m_playSectionAct, &QAction::triggered, this, &MainWindow::toggleSection);
    connect(m_pads, &PadPanel::startStopClicked, this, &MainWindow::toggleSection);

    m_loopAct = new QAction(tr("Loop"), this);
    m_loopAct->setCheckable(true);
    m_loopAct->setShortcut(Qt::Key_L);

    m_fitAct = new QAction(tr("Zoom to Fit"), this);
    m_fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_fitAct, &QAction::triggered, m_waveform, &WaveformView::zoomToFit);

    auto *zoomIn = new QAction(tr("Zoom In"), this);
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, this, [this] { m_waveform->zoomBy(0.5, m_waveform->width() / 2); });
    auto *zoomOut = new QAction(tr("Zoom Out"), this);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this] { m_waveform->zoomBy(2.0, m_waveform->width() / 2); });

    m_clearAct = new QAction(tr("Clear Selected Range"), this);
    m_clearAct->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});
    connect(m_clearAct, &QAction::triggered, this, [this] { m_model->clearRange(m_model->armed()); });

    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newAct);
    file->addAction(m_openAct);
    file->addAction(m_saveAct);
    file->addAction(m_saveAsAct);
    file->addSeparator();
    file->addAction(m_importAct);
    file->addAction(m_exportAct);
    file->addSeparator();
    QAction *quit = file->addAction(tr("Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_clearAct);
    edit->addAction(tr("Clear All Ranges"), m_model, &SectionModel::clearAll);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addAction(zoomIn);
    view->addAction(zoomOut);
    view->addAction(m_fitAct);

    QMenu *transport = menuBar()->addMenu(tr("&Transport"));
    transport->addAction(m_playAct);
    transport->addAction(m_pauseAct);
    transport->addAction(m_playPauseAct);
    transport->addAction(m_stopAct);
    transport->addSeparator();
    transport->addAction(m_playSectionAct);
    transport->addAction(m_loopAct);

    QToolBar *tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tb->addAction(m_playAct);
    tb->addAction(m_pauseAct);
    tb->addAction(m_stopAct);
}

void MainWindow::newProject()
{
    if (!maybeSave())
        return;
    resetSong();
    m_projectPath.clear();
    setWindowModified(false);
    updateTitle();
}

void MainWindow::openProjectDialog()
{
    if (!maybeSave())
        return;
    const QString start = !m_projectPath.isEmpty() ? QFileInfo(m_projectPath).absolutePath()
                                                   : QFileInfo(m_path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), start, tr(Project::kFileFilter));
    if (!path.isEmpty())
        openProject(path);
}

void MainWindow::openProject(const QString &path)
{
    Project::Data data;
    QString error;
    if (!Project::load(path, &data, &error)) {
        QMessageBox::warning(this, tr("Open failed"), tr("Could not open %1:\n%2").arg(QFileInfo(path).fileName(), error));
        return;
    }

    bool relocated = false;
    if (!data.songPath.isEmpty() && !QFileInfo::exists(data.songPath)) {
        const auto answer = QMessageBox::question(
            this, tr("Song not found"),
            tr("The song for this project was not found:\n%1\n\nLocate it?").arg(QDir::toNativeSeparators(data.songPath)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return;
        data.songPath = QFileDialog::getOpenFileName(this, tr("Locate Song"), QFileInfo(path).absolutePath(),
                                                     tr("Audio files (*.mp3 *.wav *.m4a *.flac *.ogg);;All files (*)"));
        if (data.songPath.isEmpty())
            return;
        relocated = true;
    }

    resetSong();
    m_projectPath = path;
    m_pendingRegions = data.regions;
    m_pendingRate = data.sampleRate;
    if (!data.songPath.isEmpty())
        loadSong(data.songPath);
    // A relocated song means the saved path is stale.
    setWindowModified(relocated);
    updateTitle();
}

bool MainWindow::saveProject()
{
    if (m_projectPath.isEmpty())
        return saveProjectAs();
    return writeProject(m_projectPath);
}

bool MainWindow::saveProjectAs()
{
    QString suggested = m_projectPath;
    if (suggested.isEmpty() && !m_path.isEmpty())
        suggested = QFileInfo(m_path).absoluteDir().filePath(QFileInfo(m_path).completeBaseName() + "." + Project::kSuffix);
    QString path = QFileDialog::getSaveFileName(this, tr("Save Project"), suggested, tr(Project::kFileFilter));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".") + Project::kSuffix;
    return writeProject(path);
}

bool MainWindow::writeProject(const QString &path)
{
    Project::Data data;
    data.songPath = m_path;
    data.sampleRate = m_audio.sampleRate;
    for (const Section &s : m_model->sections()) {
        if (s.isSet())
            data.regions.push_back({s.name, s.startFrame, s.endFrame});
    }

    QString error;
    if (!Project::save(path, data, &error)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save %1:\n%2").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    m_projectPath = path;
    setWindowModified(false);
    updateTitle();
    statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

// Returns false if the user cancelled.
bool MainWindow::maybeSave()
{
    if (!isWindowModified())
        return true;
    const auto answer = QMessageBox::warning(
        this, tr("Unsaved changes"),
        tr("Save changes to %1?").arg(m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    return answer == QMessageBox::Discard;
}

void MainWindow::importSongDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Song"), QFileInfo(m_path).absolutePath(),
                                                      tr("Audio files (*.mp3 *.wav *.m4a *.flac *.ogg);;All files (*)"));
    if (!path.isEmpty())
        importSong(path);
}

// Replaces the project's song; existing regions belong to the old song, so they are cleared.
void MainWindow::importSong(const QString &path)
{
    if (m_model->anySet()) {
        const auto answer = QMessageBox::question(
            this, tr("Replace song?"), tr("Importing a new song clears all pad regions."),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
            return;
    }
    resetSong();
    loadSong(path);
    setWindowModified(true);
}

void MainWindow::resetSong()
{
    m_loader->cancel();
    m_loading = false;
    m_progress->hide();
    m_player->stop();
    m_player->setSource({});
    m_sectionStartMs = m_sectionEndMs = -1;
    m_waveform->setAudio(nullptr);
    m_audio = {};
    m_path.clear();
    m_pendingRegions.clear();
    m_pendingRate = 0;

    m_trackChanges = false;
    m_model->clearAll();
    m_trackChanges = true;

    m_fileLabel->setText(tr("No song"));
    updateActions();
    updateRangeLabel();
    updateStartStop();
}

void MainWindow::loadSong(const QString &path)
{
    m_path = path;
    m_loading = true;
    m_fileLabel->setText(tr("Loading %1…").arg(QFileInfo(path).fileName()));
    m_progress->setValue(0);
    m_progress->show();
    updateActions();
    m_loader->load(path);
}

void MainWindow::onAudioLoaded(const AudioData &audio)
{
    m_audio = audio;
    m_loading = false;
    m_progress->hide();
    m_waveform->setAudio(&m_audio);
    m_pads->setSampleRate(m_audio.sampleRate);
    m_player->setSource(QUrl::fromLocalFile(m_path));
    applyPendingRegions();

    m_fileLabel->setText(QStringLiteral("%1  ·  %2  ·  %3 Hz  ·  %4 ch")
                             .arg(QFileInfo(m_path).fileName(), formatSeconds(m_audio.durationSeconds()))
                             .arg(m_audio.sampleRate)
                             .arg(m_audio.channels));
    updateActions();
    updateRangeLabel();
    updateTitle();
}

void MainWindow::applyPendingRegions()
{
    m_trackChanges = false;
    for (const Project::Region &r : std::as_const(m_pendingRegions)) {
        qint64 start = r.startFrame;
        qint64 end = r.endFrame;
        // Rescale if the decoder reports a different rate than when saved.
        if (m_pendingRate > 0 && m_pendingRate != m_audio.sampleRate) {
            start = start * m_audio.sampleRate / m_pendingRate;
            end = end * m_audio.sampleRate / m_pendingRate;
        }
        start = std::clamp<qint64>(start, 0, m_audio.frames());
        end = std::clamp<qint64>(end, 0, m_audio.frames());
        for (int i = 0; i < m_model->count(); ++i) {
            if (m_model->at(i).name == r.name && end > start)
                m_model->setRange(i, start, end);
        }
    }
    m_trackChanges = true;
    m_pendingRegions.clear();
    m_pendingRate = 0;
}

void MainWindow::onLoadFailed(const QString &message)
{
    const QString song = QFileInfo(m_path).fileName();
    // If this was a project's song, its regions never loaded; detach from the
    // file so a later Save can't overwrite it with an empty project.
    if (!m_pendingRegions.isEmpty()) {
        m_projectPath.clear();
        updateTitle();
    }
    resetSong();
    QMessageBox::warning(this, tr("Import failed"), tr("Could not load %1:\n%2").arg(song, message));
}

void MainWindow::updateTitle()
{
    const QString name = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    setWindowTitle(QStringLiteral("NXArranger — %1[*]").arg(name));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::exportRegions()
{
    if (m_audio.isEmpty() || !m_model->anySet())
        return;

    const QString dir = QFileDialog::getExistingDirectory(this, tr("Export regions to folder"),
                                                          QFileInfo(m_path).absolutePath());
    if (dir.isEmpty())
        return;

    QStringList existing;
    for (const Section &s : m_model->sections()) {
        if (s.isSet() && QFileInfo::exists(QDir(dir).filePath(s.name + ".wav")))
            existing << s.name + ".wav";
    }
    if (!existing.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, tr("Overwrite files?"),
            tr("These files already exist and will be replaced:\n\n%1").arg(existing.join('\n')),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    int written = 0;
    QStringList errors;
    for (const Section &s : m_model->sections()) {
        if (!s.isSet())
            continue;
        QString error;
        if (WavWriter::write(QDir(dir).filePath(s.name + ".wav"), m_audio, s.startFrame, s.endFrame, &error))
            ++written;
        else
            errors << QStringLiteral("%1: %2").arg(s.name, error);
    }

    if (!errors.isEmpty())
        QMessageBox::warning(this, tr("Export"), tr("Some files failed:\n\n%1").arg(errors.join('\n')));
    statusBar()->showMessage(tr("Exported %n file(s) to %1", nullptr, written).arg(QDir::toNativeSeparators(dir)), 8000);
}

void MainWindow::togglePlay()
{
    if (m_audio.isEmpty())
        return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        play();
}

void MainWindow::play()
{
    if (m_audio.isEmpty())
        return;
    m_sectionStartMs = m_sectionEndMs = -1;
    m_player->play();
    updateStartStop();
}

void MainWindow::stop()
{
    m_player->pause();
    const qint64 back = m_sectionStartMs >= 0 ? m_sectionStartMs : 0;
    m_sectionStartMs = m_sectionEndMs = -1;
    m_player->setPosition(back);
    m_waveform->setPlayhead(msToFrame(back));
    updateStartStop();
}

void MainWindow::toggleSection()
{
    if (m_sectionEndMs >= 0 && m_player->playbackState() == QMediaPlayer::PlayingState)
        stop();
    else
        playSection();
}

void MainWindow::playSection()
{
    const int armed = m_model->armed();
    if (m_audio.isEmpty() || armed < 0 || !m_model->at(armed).isSet())
        return;
    const Section &s = m_model->at(armed);
    m_sectionStartMs = frameToMs(s.startFrame);
    m_sectionEndMs = frameToMs(s.endFrame);
    m_player->setPosition(m_sectionStartMs);
    m_player->play();
    updateStartStop();
}

void MainWindow::updateStartStop()
{
    m_pads->setStartStopActive(m_sectionEndMs >= 0 && m_player->playbackState() == QMediaPlayer::PlayingState);
}

void MainWindow::onPlaybackTick()
{
    const qint64 pos = m_player->position();
    if (m_sectionEndMs >= 0 && pos >= m_sectionEndMs) {
        if (m_loopAct->isChecked()) {
            m_player->setPosition(m_sectionStartMs);
        } else {
            m_player->pause();
            m_player->setPosition(m_sectionStartMs);
            m_sectionStartMs = m_sectionEndMs = -1;
            updateStartStop();
        }
        m_waveform->setPlayhead(msToFrame(m_player->position()));
        return;
    }
    m_waveform->setPlayhead(msToFrame(pos));
}

void MainWindow::seekToFrame(qint64 frame)
{
    m_sectionStartMs = m_sectionEndMs = -1;
    m_player->setPosition(frameToMs(frame));
    m_waveform->setPlayhead(frame);
    updateStartStop();
}

void MainWindow::syncScrollBar()
{
    const qint64 total = m_waveform->totalFrames();
    const qint64 visible = m_waveform->visibleFrames();
    QSignalBlocker block(m_scroll);
    m_scroll->setRange(0, int(std::max<qint64>(0, total - visible)));
    m_scroll->setPageStep(int(std::max<qint64>(1, visible)));
    m_scroll->setSingleStep(int(std::max<qint64>(1, visible / 20)));
    m_scroll->setValue(int(m_waveform->viewStart()));
}

void MainWindow::updateActions()
{
    const bool loaded = !m_audio.isEmpty();
    const int armed = m_model->armed();
    const bool armedSet = armed >= 0 && m_model->at(armed).isSet();
    m_exportAct->setEnabled(loaded && m_model->anySet());
    const bool playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    m_playAct->setEnabled(loaded && !playing);
    m_pauseAct->setEnabled(loaded && playing);
    m_playPauseAct->setEnabled(loaded);
    m_stopAct->setEnabled(loaded);
    m_playSectionAct->setEnabled(loaded && armedSet);
    m_pads->setStartStopEnabled(loaded && armedSet);
    m_fitAct->setEnabled(loaded);
    m_clearAct->setEnabled(armedSet);
    // Saving mid-decode would drop regions that are still waiting to be applied.
    m_saveAct->setEnabled(!m_loading);
    m_saveAsAct->setEnabled(!m_loading);
}

void MainWindow::updateRangeLabel()
{
    const int armed = m_model->armed();
    if (armed < 0) {
        m_rangeLabel->clear();
        return;
    }
    const Section &s = m_model->at(armed);
    if (!s.isSet() || m_audio.isEmpty()) {
        m_rangeLabel->setText(tr("<b>%1</b>: drag on the waveform to set its range").arg(s.name));
        return;
    }
    const double a = m_audio.frameToSeconds(s.startFrame);
    const double b = m_audio.frameToSeconds(s.endFrame);
    m_rangeLabel->setText(QStringLiteral("<b>%1</b>: %2 – %3  (%4 s)")
                              .arg(s.name, formatSeconds(a), formatSeconds(b))
                              .arg(b - a, 0, 'f', 3));
}

qint64 MainWindow::msToFrame(qint64 ms) const
{
    return m_audio.sampleRate > 0 ? ms * m_audio.sampleRate / 1000 : 0;
}

qint64 MainWindow::frameToMs(qint64 frame) const
{
    return m_audio.sampleRate > 0 ? frame * 1000 / m_audio.sampleRate : 0;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.first().isLocalFile())
        return;
    const QString path = urls.first().toLocalFile();
    if (isAudioFile(path) || isProjectFile(path))
        event->acceptProposedAction();
}

// Dropping a project opens it; dropping audio imports it as the song.
void MainWindow::dropEvent(QDropEvent *event)
{
    const QString path = event->mimeData()->urls().first().toLocalFile();
    if (isProjectFile(path)) {
        if (maybeSave())
            openProject(path);
    } else {
        importSong(path);
    }
}
