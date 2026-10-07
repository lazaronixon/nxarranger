#include "PadPanel.h"

#include "SectionModel.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString padStyle(const QColor &c)
{
    // "assigned" lights the LED strip; "checked" marks the armed pad (Edit) or
    // the playing pad (Perform); "selected" marks the current variation while a
    // one-shot plays over it.
    return QStringLiteral(
               "QPushButton {"
               "  min-width: 46px; min-height: 40px;"
               "  color: #d8dbe0; font-weight: bold;"
               "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a3e46, stop:1 #24272d);"
               "  border: 1px solid #111; border-radius: 5px;"
               "  border-top: 4px solid #2b2e34;"
               "}"
               "QPushButton[assigned=\"true\"] { border-top: 4px solid %1; }"
               "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #474c55, stop:1 #2c3037); }"
               "QPushButton:checked {"
               "  color: #111;"
               "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %2, stop:1 %1);"
               "  border: 1px solid %2; border-top: 4px solid #fff;"
               "}"
               "QPushButton[selected=\"true\"] { border: 2px solid %2; border-top: 4px solid %2; }"
               "QPushButton:disabled { color: #4a4e56; background: #202227; border-top: 4px solid #2b2e34; }")
        .arg(c.name(), c.lighter(140).name());
}

QString formatSeconds(double s)
{
    const int m = int(s) / 60;
    return QStringLiteral("%1:%2").arg(m).arg(s - m * 60, 6, 'f', 3, QLatin1Char('0'));
}

} // namespace

PadPanel::PadPanel(SectionModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_group(new QButtonGroup(this))
{
    setObjectName("PadPanel");
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("#PadPanel { background: #1b1d22; border-top: 1px solid #000; }"
                  "QLabel { color: #9aa0aa; font-size: 10px; font-weight: bold; letter-spacing: 1px; }");

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(12, 10, 12, 12);
    row->setSpacing(18);

    QString currentGroup;
    QHBoxLayout *buttons = nullptr;
    for (int i = 0; i < m_model->count(); ++i) {
        const Section &s = m_model->at(i);
        if (s.group != currentGroup) {
            currentGroup = s.group;
            auto *col = new QVBoxLayout;
            col->setSpacing(4);
            auto *label = new QLabel(s.group.toUpper());
            label->setAlignment(Qt::AlignCenter);
            col->addWidget(label);
            buttons = new QHBoxLayout;
            buttons->setSpacing(4);
            col->addLayout(buttons);
            row->addLayout(col);
        }

        // Single pads (Break) are labelled by the group title above them only.
        const QString caption = s.name == s.group ? QString() : s.name.section(' ', -1);
        auto *button = new QPushButton(caption);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(padStyle(s.color));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this,
                [this, i, button](const QPoint &pos) { showPadMenu(i, button->mapToGlobal(pos)); });
        m_group->addButton(button, i);
        m_buttons.push_back(button);
        buttons->addWidget(button);
        refresh(i);
    }
    row->addStretch();

    auto *transport = new QVBoxLayout;
    transport->setSpacing(4);
    auto *transportLabel = new QLabel(tr("START/STOP"));
    transportLabel->setAlignment(Qt::AlignCenter);
    transport->addWidget(transportLabel);
    m_startStop = new QPushButton;
    m_startStop->setFocusPolicy(Qt::NoFocus);
    m_startStop->setToolTip(tr("Play the selected pad's range / stop"));
    m_startStop->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  min-width: 46px; min-height: 40px;"
        "  color: #d8dbe0; font-weight: bold;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a3e46, stop:1 #24272d);"
        "  border: 1px solid #111; border-radius: 5px; border-top: 4px solid #2b2e34;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #474c55, stop:1 #2c3037); }"
        "QPushButton:disabled { color: #5c6068; }"
        "QPushButton[active=\"true\"] { color: #111; border-top: 4px solid #fff;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7dff9a, stop:1 #2fbf55); }"));
    connect(m_startStop, &QAbstractButton::clicked, this, &PadPanel::startStopClicked);
    transport->addWidget(m_startStop);
    row->addLayout(transport);

    connect(m_group, &QButtonGroup::idClicked, this, [this](int index) {
        if (m_perform) {
            emit padTriggered(index);
            applyPerformState(); // undo the click's own toggle until the next poll
        } else {
            m_model->setArmed(index);
        }
    });

    // Queued pads blink, like the Pa3X's pending variation LED.
    m_blink = new QTimer(this);
    m_blink->setInterval(250);
    connect(m_blink, &QTimer::timeout, this, [this] {
        m_blinkOn = !m_blinkOn;
        if (m_state.queued >= 0)
            applyPerformState();
    });

    connect(m_model, &SectionModel::sectionChanged, this, &PadPanel::refresh);
    connect(m_model, &SectionModel::armedChanged, this, [this](int index) {
        if (index >= 0 && index < m_buttons.size())
            m_buttons[index]->setChecked(true);
    });
}

void PadPanel::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    for (int i = 0; i < m_buttons.size(); ++i)
        refresh(i);
}

void PadPanel::setStartStopActive(bool active)
{
    if (m_startStop->property("active").toBool() == active)
        return;
    m_startStop->setProperty("active", active);
    m_startStop->style()->unpolish(m_startStop);
    m_startStop->style()->polish(m_startStop);
}

void PadPanel::setStartStopEnabled(bool enabled)
{
    m_startStop->setEnabled(enabled);
}

void PadPanel::setPerformMode(bool on)
{
    m_perform = on;
    m_state = Arranger::State();
    m_group->setExclusive(!on);
    for (int i = 0; i < m_buttons.size(); ++i) {
        QAbstractButton *button = m_buttons[i];
        // Pads without a range can't be played.
        button->setEnabled(!on || m_model->at(i).isSet());
        setSelected(button, false);
        button->setChecked(false);
    }
    m_startStop->setToolTip(on ? tr("Start / stop the arranger") : tr("Play the selected pad's range / stop"));

    if (on) {
        m_blink->start();
    } else {
        m_blink->stop();
        m_group->setExclusive(true);
        const int armed = m_model->armed();
        if (armed >= 0 && armed < m_buttons.size())
            m_buttons[armed]->setChecked(true);
    }
}

void PadPanel::setPerformState(const Arranger::State &state)
{
    if (!m_perform)
        return;
    const bool same = state.running == m_state.running && state.playing == m_state.playing
                      && state.queued == m_state.queued && state.variation == m_state.variation
                      && state.armedIntro == m_state.armedIntro;
    m_state = state;
    if (!same)
        applyPerformState();
}

void PadPanel::applyPerformState()
{
    const Arranger::State &s = m_state;
    for (int i = 0; i < m_buttons.size(); ++i) {
        bool lit = i == s.playing || (!s.running && (i == s.variation || i == s.armedIntro));
        if (i == s.queued)
            lit = m_blinkOn;
        m_buttons[i]->setChecked(lit);
        setSelected(m_buttons[i], s.running && i == s.variation && i != s.playing && i != s.queued);
    }
}

void PadPanel::setSelected(QAbstractButton *button, bool selected)
{
    if (button->property("selected").toBool() == selected)
        return;
    button->setProperty("selected", selected);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void PadPanel::refresh(int index)
{
    const Section &s = m_model->at(index);
    QAbstractButton *button = m_buttons[index];
    button->setProperty("assigned", s.isSet());
    button->style()->unpolish(button);
    button->style()->polish(button);

    if (s.isSet() && m_sampleRate > 0) {
        const double a = double(s.startFrame) / m_sampleRate;
        const double b = double(s.endFrame) / m_sampleRate;
        button->setToolTip(QStringLiteral("%1\n%2 – %3 (%4 s)")
                               .arg(s.name, formatSeconds(a), formatSeconds(b))
                               .arg(b - a, 0, 'f', 3));
    } else {
        button->setToolTip(tr("%1\nClick to select, then drag on the waveform").arg(s.name));
    }
}

void PadPanel::showPadMenu(int index, const QPoint &globalPos)
{
    if (m_perform)
        return;
    QMenu menu(this);
    QAction *clear = menu.addAction(tr("Clear range of %1").arg(m_model->at(index).name));
    clear->setEnabled(m_model->at(index).isSet());
    if (menu.exec(globalPos) == clear)
        m_model->clearRange(index);
}
