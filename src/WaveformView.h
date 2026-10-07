#pragma once

#include "AudioData.h"

#include <QWidget>

#include <vector>

class SectionModel;

// Draws the waveform with section regions and a playhead, and lets the user
// set the armed section's range by dragging.
class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(SectionModel *model, QWidget *parent = nullptr);

    // `audio` must outlive the view or be replaced via setAudio(nullptr).
    void setAudio(const AudioData *audio);

    qint64 viewStart() const { return qint64(m_viewStart); }
    qint64 visibleFrames() const;
    qint64 totalFrames() const;

    void setViewStart(qint64 frame);
    void setPlayhead(qint64 frame);
    // Scrolls just enough to bring `frame` into view.
    void ensureVisible(qint64 frame);
    // When false (Perform mode) the mouse no longer edits regions or seeks.
    void setEditable(bool editable);
    void zoomToFit();
    void zoomBy(double factor, int anchorX);

    QSize sizeHint() const override { return {900, 260}; }
    QSize minimumSizeHint() const override { return {200, 120}; }

signals:
    void viewChanged();
    void seekRequested(qint64 frame);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    enum class Drag { None, Pending, NewRange, ResizeStart, ResizeEnd };

    void buildPeaks();
    void clampView();
    QRect waveRect() const;
    double frameToX(qint64 frame) const;
    qint64 xToFrame(double x) const;
    Drag edgeAt(int x) const;
    void peakRange(qint64 f0, qint64 f1, int &lo, int &hi) const;
    void drawRuler(QPainter &p, const QRect &r) const;

    SectionModel *m_model;
    const AudioData *m_audio = nullptr;

    static constexpr int kBinFrames = 256;
    std::vector<int16_t> m_binMin;
    std::vector<int16_t> m_binMax;

    double m_viewStart = 0.0;      // first visible frame
    double m_framesPerPixel = 1.0;
    qint64 m_playhead = -1;

    bool m_editable = true;
    Drag m_drag = Drag::None;
    int m_pressX = 0;
    qint64 m_anchorFrame = 0;
};
