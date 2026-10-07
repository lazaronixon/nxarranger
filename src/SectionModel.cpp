#include "SectionModel.h"

#include <utility>

SectionModel::SectionModel(QObject *parent)
    : QObject(parent)
{
    struct Group { const char *name; int count; QColor color; };
    const Group groups[] = {
        {"Intro", 3, QColor(0x3d, 0xa5, 0xff)},
        {"Variation", 4, QColor(0x4c, 0xd1, 0x6b)},
        {"Fill", 4, QColor(0xff, 0xb3, 0x2e)},
        {"Break", 1, QColor(0xc0, 0x6b, 0xff)},
        {"Ending", 3, QColor(0xff, 0x55, 0x55)},
    };

    for (const Group &g : groups) {
        for (int i = 1; i <= g.count; ++i) {
            Section s;
            s.group = QString::fromLatin1(g.name);
            s.name = g.count == 1 ? s.group : QStringLiteral("%1 %2").arg(s.group).arg(i);
            s.color = g.color;
            m_sections.push_back(s);
        }
    }
}

void SectionModel::setArmed(int index)
{
    if (index < -1 || index >= m_sections.size() || index == m_armed)
        return;
    m_armed = index;
    emit armedChanged(index);
}

void SectionModel::setRange(int index, qint64 start, qint64 end)
{
    if (index < 0 || index >= m_sections.size())
        return;
    if (start > end)
        std::swap(start, end);
    Section &s = m_sections[index];
    if (s.startFrame == start && s.endFrame == end)
        return;
    s.startFrame = start;
    s.endFrame = end;
    emit sectionChanged(index);
}

void SectionModel::clearRange(int index)
{
    if (index < 0 || index >= m_sections.size() || !m_sections[index].isSet())
        return;
    m_sections[index].clear();
    emit sectionChanged(index);
}

void SectionModel::clearAll()
{
    for (int i = 0; i < m_sections.size(); ++i)
        clearRange(i);
}

bool SectionModel::anySet() const
{
    for (const Section &s : m_sections)
        if (s.isSet())
            return true;
    return false;
}
