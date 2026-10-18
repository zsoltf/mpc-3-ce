/* Public grouping and layout model. It contains no host-widget ABI. */
#include "global-midi-learn-groups.h"

namespace {

constexpr MpclearnGlobalLearnGroup kGroups[MPCLEARN_GLOBAL_LEARN_GROUP_COUNT] = {
    {"PADS", 0u, 16u},
    {"PAD BANKS", 16u, 8u},
    {"Q-LINKS", 24u, 16u},
    {"TRANSPORT / NAV", 40u, 13u},
    {"GLOBAL CONTROLS", 53u, 8u},
    {"SEQUENCE / TRACK", 61u, 8u},
    {"PAD PERFORMANCE", 69u, 5u},
    {"LOOPER", 74u, 9u},
    {"STEP SEQUENCER", 83u, 2u},
    {"EDITOR TABS", 85u, 5u},
    {"MODES", 90u, 14u},
    {"PANELS", 104u, 6u},
    {"BROWSER", 110u, 7u},
    {"ADDITIONAL", 117u, 3u},
};

} // namespace

extern "C" const MpclearnGlobalLearnGroup *mpclearn_global_learn_group(
    uint32_t group) {
    return group < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT ? &kGroups[group]
                                                     : nullptr;
}

extern "C" int mpclearn_global_learn_target_location(
    uint32_t target, uint32_t *group, uint32_t *index_in_group) {
    if (target >= MPCLEARN_GLOBAL_LEARN_TARGET_COUNT || !group ||
        !index_in_group) return 0;
    for (uint32_t candidate = 0;
         candidate < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT; ++candidate) {
        const MpclearnGlobalLearnGroup &entry = kGroups[candidate];
        if (target >= entry.first_target &&
            target - entry.first_target < entry.target_count) {
            *group = candidate;
            *index_in_group = target - entry.first_target;
            return 1;
        }
    }
    return 0;
}

extern "C" int mpclearn_global_learn_layout(
    MpcUiRect bounds, MpclearnGlobalLearnLayout *layout) {
    if (!layout || bounds.x < 0 || bounds.y < 0 || bounds.width < 1280 ||
        bounds.height < 690) return 0;
    const int detail_width = bounds.width - 736;
    *layout = {
        {0, 0, bounds.width, bounds.height},
        {0, 0, bounds.width, 76},
        {24, 16, 208, 44},
        {248, 16, 48, 44},
        {304, 16, 382, 44},
        {694, 16, 48, 44},
        {768, 16, 108, 44},
        {884, 16, 108, 44},
        {1000, 16, 116, 44},
        {1124, 16, 132, 44},
        {24, 90, 270, 38},
        {310, 90, 386, 38},
        {712, 90, detail_width, 38},
        {24, 128, 270, bounds.height - 194},
        {310, 128, 386, bounds.height - 194},
        {712, 128, detail_width, bounds.height - 194},
        {728, 148, detail_width - 32, 28},
        {728, 246, detail_width - 32, 28},
        {728, 344, detail_width - 32, 28},
        {728, 394, detail_width - 32, 44},
        {728, 446, detail_width - 32, 32},
        {728, 380, 248, 44},
        {992, 380, 248, 44},
        {728, 432, 48, 44},
        {784, 432, 152, 44},
        {944, 432, 48, 44},
        {1000, 432, 240, 44},
        {728, 484, 48, 44},
        {784, 484, 152, 44},
        {944, 484, 48, 44},
        {996, 552, 244, 56},
        {24, bounds.height - 54, 672, 42},
        {728, 552, 244, 56},
        {712, bounds.height - 54, 252, 42},
        {bounds.width - 240, bounds.height - 54, 216, 42},
    };
    return 1;
}
