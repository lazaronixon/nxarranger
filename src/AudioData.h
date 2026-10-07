#pragma once

#include <cstdint>
#include <vector>

// Decoded PCM audio, interleaved signed 16-bit samples.
struct AudioData
{
    int sampleRate = 0;
    int channels = 0;
    std::vector<int16_t> samples;

    bool isEmpty() const { return samples.empty() || channels <= 0 || sampleRate <= 0; }
    int64_t frames() const { return channels > 0 ? static_cast<int64_t>(samples.size()) / channels : 0; }
    double durationSeconds() const { return sampleRate > 0 ? double(frames()) / sampleRate : 0.0; }
    double frameToSeconds(int64_t frame) const { return sampleRate > 0 ? double(frame) / sampleRate : 0.0; }
    int64_t secondsToFrame(double seconds) const { return static_cast<int64_t>(seconds * sampleRate); }
};
