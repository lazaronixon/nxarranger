#pragma once

#include "Section.h"

#include <QObject>
#include <QVector>

// Owns the Pa3X-style pads and tracks which one is armed for editing.
class SectionModel : public QObject
{
    Q_OBJECT

public:
    explicit SectionModel(QObject *parent = nullptr);

    int count() const { return m_sections.size(); }
    const Section &at(int index) const { return m_sections.at(index); }
    const QVector<Section> &sections() const { return m_sections; }

    int armed() const { return m_armed; }
    void setArmed(int index);

    void setRange(int index, qint64 start, qint64 end);
    void clearRange(int index);
    void clearAll();
    bool anySet() const;

signals:
    void sectionChanged(int index);
    void armedChanged(int index);

private:
    QVector<Section> m_sections;
    int m_armed = -1;
};
