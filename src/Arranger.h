#pragma once

#include "AudioData.h"

#include <QVector>

// Pa3X-style section sequencer. Renders interleaved int16 frames straight
// from decoded audio, so loops and switches are sample-accurate:
//   - Variations loop; pressing another variation switches at the end of the pass.
//   - Intro, Fill and Break play once and return to the current variation.
//   - Fill and Break cut in immediately.
//   - Ending is queued to the end of the pass, plays once, then stops.
// Not thread-safe; callers serialize access (see ArrangerDevice).
class Arranger
{
public:
    enum class Role { Intro, Variation, Fill, Break, Ending };

    struct Region
    {
        Role role = Role::Variation;
        qint64 start = -1;
        qint64 end = -1;
    };

    struct State
    {
        bool running = false;
        int playing = -1;    // region being played
        int queued = -1;     // region that starts at the end of the current pass
        int variation = -1;  // current variation (what one-shots return to)
        int armedIntro = -1; // intro START will play first
        qint64 frame = -1;   // next source frame to be rendered
    };

    void setAudio(const AudioData *audio);
    // Indices match SectionModel; regions without a valid range are ignored.
    void setRegions(QVector<Region> regions);
    // Length of the crossfade applied at every jump, to avoid clicks.
    void setCrossfadeFrames(int frames) { m_fadeLength = qMax(0, frames); }

    void press(int index);
    void startStop();
    void stop();

    // Fills `frames` interleaved frames; silence while stopped.
    void render(int16_t *out, qint64 frames);

    State state() const { return m_state; }

private:
    bool isValid(int index) const;
    Role roleOf(int index) const { return m_regions[index].role; }
    int playableVariation();
    void jumpTo(int index);
    void onSegmentEnd();
    void beginFade(qint64 fromFrame);

    const AudioData *m_audio = nullptr;
    QVector<Region> m_regions;
    State m_state;

    int m_fadeLength = 128;
    qint64 m_fadeFrom = 0; // source frame of the outgoing stream
    int m_fadeLeft = 0;
};
