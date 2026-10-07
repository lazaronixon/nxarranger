#include "Arranger.h"

#include <algorithm>
#include <cmath>

void Arranger::setAudio(const AudioData *audio)
{
    stop();
    m_fadeLeft = 0;
    m_audio = (audio && !audio->isEmpty()) ? audio : nullptr;
}

void Arranger::setRegions(QVector<Region> regions)
{
    stop();
    m_fadeLeft = 0;
    m_regions = std::move(regions);
    m_state.armedIntro = -1;
    m_state.variation = -1;
    playableVariation();
}

bool Arranger::isValid(int index) const
{
    if (!m_audio || index < 0 || index >= m_regions.size())
        return false;
    const Region &r = m_regions[index];
    return r.start >= 0 && r.end > r.start && r.end <= m_audio->frames();
}

// The current variation if usable, otherwise the first variation with a range.
int Arranger::playableVariation()
{
    if (isValid(m_state.variation) && roleOf(m_state.variation) == Role::Variation)
        return m_state.variation;
    for (int i = 0; i < m_regions.size(); ++i) {
        if (isValid(i) && roleOf(i) == Role::Variation) {
            m_state.variation = i;
            return i;
        }
    }
    m_state.variation = -1;
    return -1;
}

void Arranger::press(int index)
{
    if (!isValid(index))
        return;
    const bool running = m_state.running;
    const bool inEnding = running && roleOf(m_state.playing) == Role::Ending;

    switch (roleOf(index)) {
    case Role::Variation:
        m_state.variation = index;
        // Only a looping variation waits for its pass to end; one-shots
        // already return to the (new) current variation.
        if (running && roleOf(m_state.playing) == Role::Variation)
            m_state.queued = index == m_state.playing ? -1 : index;
        break;
    case Role::Intro:
        if (!running)
            m_state.armedIntro = m_state.armedIntro == index ? -1 : index;
        else if (!inEnding)
            m_state.queued = index;
        break;
    case Role::Fill:
    case Role::Break:
        if (running && !inEnding) {
            // The one-shot returns to the current variation anyway, so a
            // queued variation is redundant; a queued intro/ending stays.
            if (m_state.queued >= 0 && roleOf(m_state.queued) == Role::Variation)
                m_state.queued = -1;
            beginFade(m_state.frame);
            jumpTo(index);
        }
        break;
    case Role::Ending:
        if (running && !inEnding)
            m_state.queued = index;
        break;
    }
}

void Arranger::startStop()
{
    if (m_state.running) {
        stop();
        return;
    }
    const int target = isValid(m_state.armedIntro) ? m_state.armedIntro : playableVariation();
    if (target < 0)
        return;
    m_state.armedIntro = -1;
    m_state.queued = -1;
    m_state.running = true;
    m_fadeLeft = 0;
    jumpTo(target);
}

void Arranger::stop()
{
    if (m_state.running)
        beginFade(m_state.frame); // fade out instead of cutting
    m_state.running = false;
    m_state.playing = -1;
    m_state.queued = -1;
}

void Arranger::jumpTo(int index)
{
    m_state.playing = index;
    m_state.frame = m_regions[index].start;
}

void Arranger::beginFade(qint64 fromFrame)
{
    if (m_fadeLength <= 0 || fromFrame < 0)
        return;
    m_fadeFrom = fromFrame;
    m_fadeLeft = m_fadeLength;
}

void Arranger::onSegmentEnd()
{
    const int current = m_state.playing;
    int next = -1;
    if (roleOf(current) == Role::Ending) {
        next = -1;
    } else if (m_state.queued >= 0) {
        next = m_state.queued;
        m_state.queued = -1;
    } else if (roleOf(current) == Role::Variation) {
        next = current;
    } else {
        next = playableVariation();
    }

    if (next < 0) {
        // The ending (or last one-shot) ran out naturally; no fade needed.
        m_state.running = false;
        m_state.playing = -1;
        m_state.queued = -1;
        m_fadeLeft = 0;
        return;
    }
    beginFade(m_state.frame); // continue the source past the end while fading in
    jumpTo(next);
}

void Arranger::render(int16_t *out, qint64 frames)
{
    const int ch = m_audio ? m_audio->channels : 0;
    if (!m_audio || ch <= 0) {
        // No audio: the caller sized `out` for its own format; nothing to read.
        return;
    }
    const int16_t *src = m_audio->samples.data();
    const qint64 total = m_audio->frames();

    for (qint64 i = 0; i < frames; ++i) {
        int16_t *frameOut = out + i * ch;

        if (m_state.running && m_state.frame >= m_regions[m_state.playing].end)
            onSegmentEnd();

        // Gain of the incoming stream during a crossfade (1 when none).
        const float in = m_fadeLeft > 0 ? 1.0f - float(m_fadeLeft) / float(m_fadeLength) : 1.0f;
        const bool haveOld = m_fadeLeft > 0 && m_fadeFrom < total;

        for (int c = 0; c < ch; ++c) {
            float v = 0.0f;
            if (m_state.running)
                v += in * src[m_state.frame * ch + c];
            if (haveOld)
                v += (1.0f - in) * src[m_fadeFrom * ch + c];
            frameOut[c] = static_cast<int16_t>(std::lrint(std::clamp(v, -32768.0f, 32767.0f)));
        }

        if (m_state.running)
            ++m_state.frame;
        if (m_fadeLeft > 0) {
            ++m_fadeFrom;
            --m_fadeLeft;
        }
    }
}
