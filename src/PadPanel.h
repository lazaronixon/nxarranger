#pragma once

#include <QVector>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class SectionModel;

// Korg Pa3X-style pad buttons: Intro, Variation, Fill, Break, Ending.
class PadPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PadPanel(SectionModel *model, QWidget *parent = nullptr);

    // Converts a frame position to seconds for tooltips.
    void setSampleRate(int sampleRate);

private:
    void refresh(int index);
    void showPadMenu(int index, const QPoint &globalPos);

    SectionModel *m_model;
    QButtonGroup *m_group;
    QVector<QAbstractButton *> m_buttons;
    int m_sampleRate = 0;
};
