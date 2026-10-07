#include "WaveformView.h"

#include "SectionModel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kRulerHeight = 22;
constexpr int kLabelHeight = 18;
constexpr int kEdgeGrab = 5;
constexpr int kDragThreshold = 3;
constexpr double kMinFramesPerPixel = 1.0 / 16.0;

const QColor kBackground(0x16, 0x18, 0x1d);
const QColor kWaveColor(0x8f, 0xd3, 0xff);
const QColor kRulerColor(0x24, 0x27, 0x2e);
const QColor kTextColor(0xc8, 0xcc, 0xd4);
const QColor kPlayheadColor(0xff, 0xff, 0xff);

QString formatTime(double seconds, bool withMillis)
{
    const int total = int(seconds);
    const int m = total / 60;
    const int s = total % 60;
    if (!withMillis)
        return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
    const int ms = int(std::lround((seconds - total) * 1000.0)) % 1000;
    return QStringLiteral("%1:%2.%3").arg(m).arg(s, 2, 10, QLatin1Char('0')).arg(ms, 3, 10, QLatin1Char('0'));
}

} // namespace

WaveformView::WaveformView(SectionModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    connect(m_model, &SectionModel::sectionChanged, this, qOverload<>(&QWidget::update));
    connect(m_model, &SectionModel::armedChanged, this, qOverload<>(&QWidget::update));
}

void WaveformView::setAudio(const AudioData *audio)
{
    m_audio = (audio && !audio->isEmpty()) ? audio : nullptr;
    m_playhead = m_audio ? 0 : -1;
    buildPeaks();
    zoomToFit();
}

qint64 WaveformView::totalFrames() const
{
    return m_audio ? m_audio->frames() : 0;
}

qint64 WaveformView::visibleFrames() const
{
    return std::min<qint64>(totalFrames(), qint64(std::ceil(waveRect().width() * m_framesPerPixel)));
}

void WaveformView::setViewStart(qint64 frame)
{
    m_viewStart = double(frame);
    clampView();
    update();
    emit viewChanged();
}

void WaveformView::setPlayhead(qint64 frame)
{
    if (frame == m_playhead)
        return;
    m_playhead = frame;
    update();
}

void WaveformView::zoomToFit()
{
    const int w = std::max(1, waveRect().width());
    m_viewStart = 0.0;
    m_framesPerPixel = m_audio ? std::max(kMinFramesPerPixel, double(totalFrames()) / w) : 1.0;
    update();
    emit viewChanged();
}

void WaveformView::zoomBy(double factor, int anchorX)
{
    if (!m_audio)
        return;
    const double anchorFrame = m_viewStart + (anchorX - waveRect().left()) * m_framesPerPixel;
    const double maxFpp = std::max(kMinFramesPerPixel, double(totalFrames()) / std::max(1, waveRect().width()));
    m_framesPerPixel = std::clamp(m_framesPerPixel * factor, kMinFramesPerPixel, maxFpp);
    m_viewStart = anchorFrame - (anchorX - waveRect().left()) * m_framesPerPixel;
    clampView();
    update();
    emit viewChanged();
}

void WaveformView::buildPeaks()
{
    m_binMin.clear();
    m_binMax.clear();
    if (!m_audio)
        return;

    const qint64 frames = m_audio->frames();
    const int ch = m_audio->channels;
    const qint64 bins = (frames + kBinFrames - 1) / kBinFrames;
    m_binMin.resize(size_t(bins));
    m_binMax.resize(size_t(bins));

    const int16_t *data = m_audio->samples.data();
    for (qint64 b = 0; b < bins; ++b) {
        const qint64 f0 = b * kBinFrames;
        const qint64 f1 = std::min(frames, f0 + kBinFrames);
        int16_t lo = 0, hi = 0;
        for (const int16_t *p = data + f0 * ch, *e = data + f1 * ch; p != e; ++p) {
            lo = std::min(lo, *p);
            hi = std::max(hi, *p);
        }
        m_binMin[size_t(b)] = lo;
        m_binMax[size_t(b)] = hi;
    }
}

void WaveformView::clampView()
{
    const double maxStart = std::max(0.0, double(totalFrames()) - waveRect().width() * m_framesPerPixel);
    m_viewStart = std::clamp(m_viewStart, 0.0, maxStart);
}

QRect WaveformView::waveRect() const
{
    return rect().adjusted(0, kRulerHeight, 0, 0);
}

double WaveformView::frameToX(qint64 frame) const
{
    return waveRect().left() + (double(frame) - m_viewStart) / m_framesPerPixel;
}

qint64 WaveformView::xToFrame(double x) const
{
    const double f = m_viewStart + (x - waveRect().left()) * m_framesPerPixel;
    return std::clamp<qint64>(qint64(std::llround(f)), 0, totalFrames());
}

// Min/max of all channels over frames [f0, f1), using the bin cache when the
// range is wide enough and raw samples otherwise.
void WaveformView::peakRange(qint64 f0, qint64 f1, int &lo, int &hi) const
{
    lo = 0;
    hi = 0;
    const qint64 frames = totalFrames();
    f0 = std::clamp<qint64>(f0, 0, frames);
    f1 = std::clamp<qint64>(std::max(f1, f0 + 1), 0, frames);
    if (f0 >= f1)
        return;

    if (f1 - f0 >= kBinFrames) {
        const qint64 b0 = f0 / kBinFrames;
        const qint64 b1 = std::min<qint64>(qint64(m_binMin.size()), (f1 + kBinFrames - 1) / kBinFrames);
        for (qint64 b = b0; b < b1; ++b) {
            lo = std::min<int>(lo, m_binMin[size_t(b)]);
            hi = std::max<int>(hi, m_binMax[size_t(b)]);
        }
        return;
    }

    const int ch = m_audio->channels;
    const int16_t *data = m_audio->samples.data();
    for (const int16_t *p = data + f0 * ch, *e = data + f1 * ch; p != e; ++p) {
        lo = std::min<int>(lo, *p);
        hi = std::max<int>(hi, *p);
    }
}

void WaveformView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), kBackground);

    const QRect ruler(0, 0, width(), kRulerHeight);
    p.fillRect(ruler, kRulerColor);

    const QRect wr = waveRect();
    if (!m_audio) {
        p.setPen(kTextColor);
        p.drawText(wr, Qt::AlignCenter, tr("Open an MP3 file (File ▸ Open or drag it here)"));
        return;
    }

    drawRuler(p, ruler);

    // Section regions.
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.9);
    for (int i = 0; i < m_model->count(); ++i) {
        const Section &s = m_model->at(i);
        if (!s.isSet())
            continue;
        const bool armed = i == m_model->armed();
        const double x0 = frameToX(s.startFrame);
        const double x1 = frameToX(s.endFrame);
        if (x1 < wr.left() || x0 > wr.right())
            continue;

        QRectF region(x0, wr.top(), std::max(1.0, x1 - x0), wr.height());
        QColor fill = s.color;
        fill.setAlpha(armed ? 90 : 45);
        p.fillRect(region, fill);

        QColor edge = s.color;
        edge.setAlpha(armed ? 255 : 150);
        p.setPen(QPen(edge, armed ? 2 : 1));
        p.drawLine(QPointF(x0, wr.top()), QPointF(x0, wr.bottom()));
        p.drawLine(QPointF(x1, wr.top()), QPointF(x1, wr.bottom()));

        QRectF label(x0, wr.top(), std::max(0.0, x1 - x0), kLabelHeight);
        QColor labelBg = s.color;
        labelBg.setAlpha(armed ? 230 : 140);
        p.fillRect(label, labelBg);
        p.setFont(labelFont);
        p.setPen(Qt::black);
        p.drawText(label.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(s.name, Qt::ElideRight, int(label.width()) - 6));

        if (armed) {
            p.setBrush(edge);
            p.setPen(Qt::NoPen);
            const double hy = wr.center().y();
            p.drawRoundedRect(QRectF(x0 - 3, hy - 12, 6, 24), 2, 2);
            p.drawRoundedRect(QRectF(x1 - 3, hy - 12, 6, 24), 2, 2);
            p.setBrush(Qt::NoBrush);
        }
    }

    // Waveform.
    const double mid = wr.top() + kLabelHeight + (wr.height() - kLabelHeight) / 2.0;
    const double half = (wr.height() - kLabelHeight) / 2.0 - 2.0;
    p.setPen(kWaveColor);
    for (int x = wr.left(); x <= wr.right(); ++x) {
        const double fa = m_viewStart + (x - wr.left()) * m_framesPerPixel;
        const qint64 f0 = qint64(std::floor(fa));
        const qint64 f1 = qint64(std::floor(fa + m_framesPerPixel));
        if (f0 >= totalFrames())
            break;
        int lo, hi;
        peakRange(f0, f1, lo, hi);
        const double y0 = mid - hi / 32768.0 * half;
        const double y1 = mid - lo / 32768.0 * half;
        p.drawLine(QPointF(x + 0.5, y0), QPointF(x + 0.5, std::max(y1, y0 + 1.0)));
    }
    p.setPen(QColor(255, 255, 255, 40));
    p.drawLine(QPointF(wr.left(), mid), QPointF(wr.right(), mid));

    // Playhead.
    if (m_playhead >= 0) {
        const double x = frameToX(m_playhead);
        if (x >= wr.left() && x <= wr.right()) {
            p.setPen(QPen(kPlayheadColor, 1.5));
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
        }
    }
}

void WaveformView::drawRuler(QPainter &p, const QRect &r) const
{
    const double sr = m_audio->sampleRate;
    const double secondsPerPixel = m_framesPerPixel / sr;

    // Pick a tick step that leaves at least ~80 px between labels.
    static const double steps[] = {0.001, 0.005, 0.01, 0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300};
    double step = steps[std::size(steps) - 1];
    for (double s : steps) {
        if (s / secondsPerPixel >= 80.0) {
            step = s;
            break;
        }
    }

    const double t0 = m_viewStart / sr;
    const double t1 = t0 + r.width() * secondsPerPixel;
    p.setPen(kTextColor);
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 0.85);
    p.setFont(f);
    for (double t = std::ceil(t0 / step) * step; t <= t1; t += step) {
        const double x = r.left() + (t - t0) / secondsPerPixel;
        p.drawLine(QPointF(x, r.bottom() - 6), QPointF(x, r.bottom()));
        p.drawText(QPointF(x + 3, r.bottom() - 7), formatTime(t, step < 1.0));
    }
}

void WaveformView::resizeEvent(QResizeEvent *)
{
    if (!m_audio)
        return;
    const double maxFpp = std::max(kMinFramesPerPixel, double(totalFrames()) / std::max(1, waveRect().width()));
    m_framesPerPixel = std::min(m_framesPerPixel, maxFpp);
    clampView();
    emit viewChanged();
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    if (!m_audio)
        return;
    const QPoint angle = event->angleDelta();
    const QPoint pixels = event->pixelDelta();
    const bool horizontal = std::abs(angle.x()) > std::abs(angle.y()) || (event->modifiers() & Qt::ShiftModifier);

    if (horizontal) {
        int dx = !pixels.isNull() ? pixels.x() : angle.x() / 4;
        if (dx == 0)
            dx = !pixels.isNull() ? pixels.y() : angle.y() / 4;
        setViewStart(qint64(m_viewStart - dx * m_framesPerPixel));
    } else {
        zoomBy(std::pow(1.0015, -angle.y()), int(event->position().x()));
    }
    event->accept();
}

WaveformView::Drag WaveformView::edgeAt(int x) const
{
    const int armed = m_model->armed();
    if (armed < 0 || !m_model->at(armed).isSet())
        return Drag::None;
    const Section &s = m_model->at(armed);
    const double dStart = std::abs(x - frameToX(s.startFrame));
    const double dEnd = std::abs(x - frameToX(s.endFrame));
    if (dStart <= kEdgeGrab && dStart <= dEnd)
        return Drag::ResizeStart;
    if (dEnd <= kEdgeGrab)
        return Drag::ResizeEnd;
    return Drag::None;
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (!m_audio || event->button() != Qt::LeftButton)
        return;
    const int x = int(event->position().x());
    m_pressX = x;
    const Drag edge = edgeAt(x);
    m_drag = edge != Drag::None ? edge : Drag::Pending;
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_audio)
        return;
    const int x = int(event->position().x());
    const int armed = m_model->armed();

    if (m_drag == Drag::None) {
        setCursor(edgeAt(x) != Drag::None ? Qt::SizeHorCursor : Qt::IBeamCursor);
        return;
    }

    if (m_drag == Drag::Pending) {
        if (std::abs(x - m_pressX) < kDragThreshold || armed < 0)
            return;
        m_drag = Drag::NewRange;
        m_anchorFrame = xToFrame(m_pressX);
    }

    const qint64 frame = xToFrame(x);
    const Section &s = m_model->at(armed);
    switch (m_drag) {
    case Drag::NewRange:
        m_model->setRange(armed, std::min(m_anchorFrame, frame), std::max(m_anchorFrame, frame));
        break;
    case Drag::ResizeStart:
        if (frame > s.endFrame) {
            m_model->setRange(armed, s.endFrame, frame);
            m_drag = Drag::ResizeEnd;
        } else {
            m_model->setRange(armed, frame, s.endFrame);
        }
        break;
    case Drag::ResizeEnd:
        if (frame < s.startFrame) {
            m_model->setRange(armed, frame, s.startFrame);
            m_drag = Drag::ResizeStart;
        } else {
            m_model->setRange(armed, s.startFrame, frame);
        }
        break;
    default:
        break;
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_audio || event->button() != Qt::LeftButton)
        return;
    if (m_drag == Drag::Pending)
        emit seekRequested(xToFrame(event->position().x()));

    // A zero-length drag leaves an empty range; treat it as "not set".
    const int armed = m_model->armed();
    if (armed >= 0 && m_model->at(armed).startFrame >= 0 && !m_model->at(armed).isSet())
        m_model->clearRange(armed);
    m_drag = Drag::None;
}

void WaveformView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_audio)
        return;
    // Double-click a region to arm its pad; prefer the shortest region under the cursor.
    const qint64 frame = xToFrame(event->position().x());
    int best = -1;
    qint64 bestLen = 0;
    for (int i = 0; i < m_model->count(); ++i) {
        const Section &s = m_model->at(i);
        if (s.isSet() && frame >= s.startFrame && frame <= s.endFrame) {
            const qint64 len = s.endFrame - s.startFrame;
            if (best < 0 || len < bestLen) {
                best = i;
                bestLen = len;
            }
        }
    }
    if (best >= 0)
        m_model->setArmed(best);
}
