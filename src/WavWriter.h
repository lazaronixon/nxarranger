#pragma once

#include "AudioData.h"

#include <QString>

namespace WavWriter {

// Writes frames [startFrame, endFrame) of `audio` as a 16-bit PCM WAV file.
// Returns false and fills `error` on failure.
bool write(const QString &path, const AudioData &audio, int64_t startFrame, int64_t endFrame,
           QString *error = nullptr);

} // namespace WavWriter
