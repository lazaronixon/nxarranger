#include "PadPanel.h"

#include "SectionModel.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString padStyle(const QColor &c)
{
    // "assigned" lights the LED strip; "checked" marks the armed pad.
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
               "}")
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

        const QString caption = s.name == s.group ? s.group.toUpper() : s.name.section(' ', -1);
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

    connect(m_group, &QButtonGroup::idClicked, m_model, &SectionModel::setArmed);
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
    QMenu menu(this);
    QAction *clear = menu.addAction(tr("Clear range of %1").arg(m_model->at(index).name));
    clear->setEnabled(m_model->at(index).isSet());
    if (menu.exec(globalPos) == clear)
        m_model->clearRange(index);
}
