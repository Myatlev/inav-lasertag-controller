# Prototype v0

## Scope

This first firmware prototype is only for bench bring-up with the flight controller.

It implements:

- UART frame parsing
- `READY`
- `PING`
- `SET_PLAYER_ID`
- `FIRE`
- `DEBUG`
- manual `HIT` emulation from USB console

It does not implement:

- IR TX
- IR RX
- hardware timing for emitters
- interrupts
- outdoor signal handling

## Purpose

The goal is to prove the software handshake between:

- `iNav`
- `iNav Configurator`
- `inav-lasertag-controller`

before adding real IR hardware behavior.

## Expected bench flow

1. `ESP8266 NodeMCU v3` boots.
2. Controller sends `READY`.
3. Flight controller configures `player_id`.
4. Flight controller sends `FIRE`.
5. Controller replies with `DEBUG` confirming fire reception.
6. Developer types `hit` or `hit <id>` in the USB console.
7. Controller sends synthetic `HIT` to the flight controller.

## Current hardware target

- board family: `ESP8266`
- development board: `NodeMCU v3`
- PlatformIO board id: `nodemcuv2`
- FC UART link: `SoftwareSerial` on GPIO12 / GPIO14
