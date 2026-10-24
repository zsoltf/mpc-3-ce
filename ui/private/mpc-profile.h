#ifndef MPCLEARN_UI_MPC_PROFILE_H
#define MPCLEARN_UI_MPC_PROFILE_H
/* The admitted production build selects one profile before compilation.
 * This is not runtime firmware detection or fallback. */
#ifdef MODEL_FIRMWARE_310
#include "mpc-3.10-profile.h"
#define MPC_UI_PROFILE_SELECTED MPC_UI_PROFILE_MPC_3_10
#else
#include "mpc-3.9.1-profile.h"
#define MPC_UI_PROFILE_SELECTED MPC_UI_PROFILE_MPC_3_9_1
#endif
#endif
