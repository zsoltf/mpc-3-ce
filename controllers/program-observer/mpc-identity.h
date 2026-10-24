#ifndef MPC_IDENTITY_H
#define MPC_IDENTITY_H
#if defined(MODEL_FIRMWARE_310) || defined(MODEL_OFFLINE_310)
#include "model-310-identity.h"
#else
#define MPC_EXECUTABLE_SIZE 112222004
#define MPC_EXECUTABLE_SHA256 "2fa5e9818c3a3beedfcb3fafe34f5b3f9a07295a3b68fb9e7949be6b64fa8daf"
#define MPC_EXECUTABLE_BUILD_ID_BYTES {0x83,0x71,0x19,0x9f,0x94,0x5e,0xcc,0x40,0xf7,0x9c,0xb1,0x9c,0x17,0xff,0x5d,0xe7,0x28,0x10,0xa9,0x1c}
#endif
#endif
