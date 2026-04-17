#include <Arduino.h>
#include <SoftwareSerial.h>

#include "lt_protocol.h"

namespace {

constexpr uint32_t kUsbBaud = LT_CONTROLLER_USB_BAUD;
constexpr uint32_t kFcBaud = LT_CONTROLLER_UART_BAUD;
constexpr int kFcRxPin = LT_CONTROLLER_UART_RX_PIN;
constexpr int kFcTxPin = LT_CONTROLLER_UART_TX_PIN;

SoftwareSerial fcSerial(kFcRxPin, kFcTxPin, false);

enum class ParseState : uint8_t {
    WaitSof,
    ReadVersion,
    ReadType,
    ReadLength,
    ReadPayload,
    ReadCrc,
};

struct Parser {
    ParseState state = ParseState::WaitSof;
    lt::Frame frame;
    uint8_t payloadIndex = 0;
};

Parser parser;

uint8_t localPlayerId = 0;
uint8_t fireSeq = 0;
String consoleLine;

void resetParser()
{
    parser.state = ParseState::WaitSof;
    parser.frame = {};
    parser.payloadIndex = 0;
}

void sendFrame(uint8_t type, const uint8_t *payload, uint8_t length)
{
    uint8_t header[4] = {
        lt::kSof,
        lt::kVersion,
        type,
        length,
    };

    fcSerial.write(header, sizeof(header));

    if (length > 0 && payload != nullptr) {
        fcSerial.write(payload, length);
    }

    uint8_t crcBuffer[3 + lt::kMaxPayloadSize] = {
        lt::kVersion,
        type,
        length,
    };

    if (length > 0 && payload != nullptr) {
        memcpy(&crcBuffer[3], payload, length);
    }

    const uint8_t crc = lt::crc8(crcBuffer, 3 + length);
    fcSerial.write(crc);
}

void sendReady()
{
    lt::ReadyPayload ready;
    ready.fwMajor = 0;
    ready.fwMinor = 1;
    ready.capabilities = 0;

    sendFrame(lt::MSG_READY, reinterpret_cast<const uint8_t *>(&ready), sizeof(ready));
}

void sendDebug(uint8_t code, uint16_t value)
{
    lt::DebugPayload payload;
    payload.code = code;
    payload.value = value;

    sendFrame(lt::MSG_DEBUG, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload));
}

void sendHit(uint8_t shooterId, uint8_t rxQuality, uint8_t seq)
{
    lt::HitPayload payload;
    payload.shooterId = shooterId;
    payload.rxQuality = rxQuality;
    payload.seq = seq;

    sendFrame(lt::MSG_HIT, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload));
}

void printHelp()
{
    Serial.println(F("Commands:"));
    Serial.println(F("  help"));
    Serial.println(F("  status"));
    Serial.println(F("  hit"));
    Serial.println(F("  hit <id>"));
    Serial.println(F("  ready"));
}

void printStatus()
{
    Serial.print(F("player_id="));
    Serial.println(localPlayerId);
    Serial.print(F("last_fire_seq="));
    Serial.println(fireSeq);
}

void handleConsoleCommand(const String &line)
{
    if (line.equalsIgnoreCase("help")) {
        printHelp();
        return;
    }

    if (line.equalsIgnoreCase("status")) {
        printStatus();
        return;
    }

    if (line.equalsIgnoreCase("ready")) {
        sendReady();
        sendDebug(lt::DEBUG_BOOT, localPlayerId);
        Serial.println(F("READY sent"));
        return;
    }

    if (line.equalsIgnoreCase("hit")) {
        sendHit(localPlayerId, 255, fireSeq);
        sendDebug(lt::DEBUG_HIT_EMITTED, localPlayerId);
        Serial.println(F("HIT sent with local player_id"));
        return;
    }

    if (line.startsWith("hit ")) {
        const int requestedId = line.substring(4).toInt();

        if (requestedId < 0 || requestedId > 7) {
            Serial.println(F("player_id must be in range 0..7"));
            return;
        }

        sendHit(static_cast<uint8_t>(requestedId), 255, fireSeq);
        sendDebug(lt::DEBUG_HIT_EMITTED, static_cast<uint8_t>(requestedId));
        Serial.println(F("HIT sent"));
        return;
    }

    Serial.println(F("Unknown command. Type 'help'."));
}

void handleIncomingFrame(const lt::Frame &frame)
{
    if (frame.version != lt::kVersion) {
        sendDebug(lt::DEBUG_BAD_FRAME, frame.version);
        return;
    }

    switch (frame.type) {
    case lt::MSG_PING:
        sendDebug(lt::DEBUG_PING_RECEIVED, localPlayerId);
        break;

    case lt::MSG_SET_PLAYER_ID:
        if (frame.length >= 1) {
            localPlayerId = static_cast<uint8_t>(frame.payload[0] & 0x07);
            sendDebug(lt::DEBUG_PLAYER_ID_SET, localPlayerId);
        } else {
            sendDebug(lt::DEBUG_BAD_FRAME, frame.type);
        }
        break;

    case lt::MSG_FIRE:
        if (frame.length >= 2) {
            fireSeq = frame.payload[0];
            const uint16_t packed = static_cast<uint16_t>(frame.payload[0]) |
                                    (static_cast<uint16_t>(frame.payload[1]) << 8);
            sendDebug(lt::DEBUG_FIRE_RECEIVED, packed);
        } else {
            sendDebug(lt::DEBUG_BAD_FRAME, frame.type);
        }
        break;

    default:
        sendDebug(lt::DEBUG_BAD_FRAME, frame.type);
        break;
    }
}

void processFcByte(uint8_t byteValue)
{
    switch (parser.state) {
    case ParseState::WaitSof:
        if (byteValue == lt::kSof) {
            resetParser();
            parser.state = ParseState::ReadVersion;
        }
        break;

    case ParseState::ReadVersion:
        parser.frame.version = byteValue;
        parser.state = ParseState::ReadType;
        break;

    case ParseState::ReadType:
        parser.frame.type = byteValue;
        parser.state = ParseState::ReadLength;
        break;

    case ParseState::ReadLength:
        parser.frame.length = byteValue;

        if (parser.frame.length > lt::kMaxPayloadSize) {
            sendDebug(lt::DEBUG_BAD_FRAME, parser.frame.length);
            resetParser();
        } else if (parser.frame.length == 0) {
            parser.state = ParseState::ReadCrc;
        } else {
            parser.payloadIndex = 0;
            parser.state = ParseState::ReadPayload;
        }
        break;

    case ParseState::ReadPayload:
        parser.frame.payload[parser.payloadIndex++] = byteValue;

        if (parser.payloadIndex >= parser.frame.length) {
            parser.state = ParseState::ReadCrc;
        }
        break;

    case ParseState::ReadCrc: {
        uint8_t crcBuffer[3 + lt::kMaxPayloadSize] = {
            parser.frame.version,
            parser.frame.type,
            parser.frame.length,
        };

        if (parser.frame.length > 0) {
            memcpy(&crcBuffer[3], parser.frame.payload, parser.frame.length);
        }

        const uint8_t computed = lt::crc8(crcBuffer, 3 + parser.frame.length);

        if (computed == byteValue) {
            handleIncomingFrame(parser.frame);
        } else {
            sendDebug(lt::DEBUG_BAD_FRAME, parser.frame.type);
        }

        resetParser();
        break;
    }
    }
}

void processUsbConsole()
{
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            if (!consoleLine.isEmpty()) {
                handleConsoleCommand(consoleLine);
                consoleLine = "";
            }
            continue;
        }

        consoleLine += ch;
    }
}

} // namespace

void setup()
{
    Serial.begin(kUsbBaud);
    fcSerial.begin(kFcBaud);

    delay(200);

    Serial.println(F("inav-lasertag-controller prototype boot (ESP8266)"));
    Serial.print(F("FC UART RX pin: "));
    Serial.println(kFcRxPin);
    Serial.print(F("FC UART TX pin: "));
    Serial.println(kFcTxPin);
    printHelp();

    sendReady();
    sendDebug(lt::DEBUG_BOOT, localPlayerId);
}

void loop()
{
    while (fcSerial.available() > 0) {
        processFcByte(static_cast<uint8_t>(fcSerial.read()));
    }

    processUsbConsole();
}
