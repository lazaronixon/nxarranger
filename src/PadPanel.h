#pragma once

#include "Arranger.h"

#include <QVector>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QTimer;
class SectionModel;

// Korg Pa3X-style pad buttons: Intro, Variation, Fill, Break, Ending.
class PadPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PadPanel(SectionModel *model, QWidget *parent = nullptr);

    // Converts a frame position to seconds for tooltips.
    void setSampleRate(int sampleRate);

    // START/STOP pad: lit while the armed section (Edit) or arranger (Perform) plays.
    void setStartStopActive(bool active);
    void setStartStopEnabled(bool enabled);

    // In Perform mode clicks emit padTriggered() instead of arming a pad for
    // editing, and the lights follow setPerformState().
    void setPerformMode(bool on);
    void setPerformState(const Arranger::State &state);

signals:
    void startStopClicked();
    void padTriggered(int index);

private:
    void refresh(int index);
    void showPadMenu(int index, const QPoint &globalPos);
    void applyPerformState();
    void setSelected(QAbstractButton *button, bool selected);

    SectionModel *m_model;
    QButtonGroup *m_group;
    QVector<QAbstractButton *> m_buttons;
    QAbstractButton *m_startStop = nullptr;
    int m_sampleRate = 0;

    bool m_perform = false;
    Arranger::State m_state;
    QTimer *m_blink = nullptr;
    bool m_blinkOn = false;
};
