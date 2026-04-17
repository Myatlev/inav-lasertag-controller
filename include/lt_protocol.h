#pragma once

#include <Arduino.h>

namespace lt {

constexpr uint8_t kSof = 0xA5;
constexpr uint8_t kVersion = 0x01;

enum MessageType : uint8_t {
    MSG_PING = 0x01,
    MSG_SET_PLAYER_ID = 0x02,
    MSG_FIRE = 0x03,

    MSG_READY = 0x80,
    MSG_HIT = 0x81,
    MSG_DEBUG = 0xF0,
};

constexpr uint8_t kMaxPayloadSize = 32;

struct Frame {
    uint8_t version = kVersion;
    uint8_t type = 0;
    uint8_t length = 0;
    uint8_t payload[kMaxPayloadSize] = {};
};

struct FireCommand {
    uint8_t seq = 0;
    uint8_t flags = 0;
};

struct ReadyPayload {
    uint8_t fwMajor = 0;
    uint8_t fwMinor = 1;
    uint16_t capabilities = 0;
};

struct HitPayload {
    uint8_t shooterId = 0;
    uint8_t rxQuality = 255;
    uint8_t seq = 0;
};

struct DebugPayload {
    uint8_t code = 0;
    uint16_t value = 0;
};

enum DebugCode : uint8_t {
    DEBUG_BOOT = 0x01,
    DEBUG_FIRE_RECEIVED = 0x02,
    DEBUG_HIT_EMITTED = 0x03,
    DEBUG_BAD_FRAME = 0x04,
    DEBUG_PLAYER_ID_SET = 0x05,
    DEBUG_PING_RECEIVED = 0x06,
};

inline uint8_t crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0x00;

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x07);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

} // namespace lt
