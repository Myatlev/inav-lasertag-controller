# inav-lasertag-controller

Reference external controller firmware for the `inav-lasertag` project.

This repository hosts the firmware for the UART-connected laser tag controller that sits next to the flight controller. For the current MVP prototype, the target is `ESP8266 NodeMCU v3`, and the first goal is not IR combat itself, but a reliable controller <-> FC integration path.

## Prototype goal

The first prototype focuses on communication only:

- announce controller presence with `READY`;
- accept `SET_PLAYER_ID`;
- accept `FIRE`;
- send `DEBUG` feedback when a fire command is received;
- allow manual hit emulation from the USB console;
- avoid any gameplay logic on the controller.

## Non-goals of the first prototype

- no real IR transmitter yet;
- no real IR receiver yet;
- no local scoring;
- no respawn or match logic;
- no hardware-specific optics work.

## Current tech choice

- target: `ESP8266 NodeMCU v3`
- PlatformIO board: `nodemcuv2`
- framework: Arduino
- build system: PlatformIO

## UART model

The controller uses:

- `SoftwareSerial` for the flight controller UART link;
- `Serial` for USB debug console and manual test commands.

Default FC UART pins for the current prototype:

- FC RX on controller side: GPIO12 (`D6`)
- FC TX on controller side: GPIO14 (`D5`)

The default protocol framing matches the current `inav-lasertag-spec` `UART Protocol v0.1`.

## Prototype console commands

Available over the USB serial console:

- `help`
- `status`
- `hit`
- `hit <id>`
- `ready`

These commands are only for bring-up and bench testing.