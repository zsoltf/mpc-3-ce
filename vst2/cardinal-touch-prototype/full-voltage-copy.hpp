/* Exact active-channel transfer for the pinned Cardinal engine.
 * No arithmetic, rounded-width overcopy, or change to port channel lifetime. */
#pragma once
#include <cstring>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace mpclearn_cardinal {
inline void copyVoltages(float* destination, const float* source, int channels,
                         int previousChannels) {
    int channel = 0;
    for (; channel + 4 <= channels; channel += 4) {
#if defined(__ARM_NEON)
        vst1q_u8(reinterpret_cast<uint8_t*>(destination + channel),
                 vld1q_u8(reinterpret_cast<const uint8_t*>(source + channel)));
#else
        std::memcpy(destination + channel, source + channel, 4 * sizeof(float));
#endif
    }
    if (channels - channel >= 2) {
#if defined(__ARM_NEON)
        vst1_u8(reinterpret_cast<uint8_t*>(destination + channel),
                vld1_u8(reinterpret_cast<const uint8_t*>(source + channel)));
#else
        std::memcpy(destination + channel, source + channel, 2 * sizeof(float));
#endif
        channel += 2;
    }
    if (channel < channels)
        std::memcpy(destination + channel, source + channel, sizeof(float));
    for (int index = channels; index < previousChannels; ++index)
        destination[index] = 0.f;
}
}
