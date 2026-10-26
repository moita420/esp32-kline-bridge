# esp32-kline-bridge

A Wi-Fi bridge to the K-line diagnostic bus of older VAG cars (KWP1281), built on an
ESP32 and an ST **L9637D** K-line transceiver. It exposes the ECU's diagnostic session
as a small HTTP/JSON API and a browser UI, so you can read identification, measuring
values and memory from a phone or laptop without a diagnostic cable.

Tested on a Bosch EDC15 diesel ECU (VW Bora 1.9 TDI) on the bench and in the car.

## What it does

- **5-baud slow init** bit-banged on the TX pin, sync byte (`0x55`) detection, keyword
  exchange and the inverted-KW2 reply.
- **KWP1281 block protocol**: per-byte complement handshake, block counter, ACK
  keep-alive, timeouts and error reporting.
- **Read-only diagnostics**: identification blocks, RAM/ROM read requests
  (titles `0x01`/`0x03`), range snapshots, and a passive **sniff mode** that only
  listens to the bus.
- **Web UI** served from the ESP32 (`/`), plus a JSON API:

| Endpoint | Method | Purpose |
|---|---|---|
| `/connect?baud=` | POST | slow init + read ID blocks |
| `/status` | GET | keywords, part number, baud |
| `/read?addr=&len=` | GET | RAM read request |
| `/read_rom?addr=&len=` | GET | ROM read request |
| `/snapshot?start=&end=` | GET | read a range in runs of 8 bytes |
| `/sniff?ms=` | GET | passive capture of raw bus bytes |
| `/disconnect` | POST | end session |

## Hardware

```
ESP32 GPIO17 (TX) ──► level shifter ──► L9637D TXD
ESP32 GPIO16 (RX) ◄── L9637D RXD (open-drain, 10 kΩ pull-up to 3.3 V)
ESP32 GPIO4       ──► L9637D NSLP (high = awake)
L9637D K  ──── OBD pin 7      L9637D VS ── OBD pin 16 (+12 V)      GND ── OBD pins 4/5
```

`hardware/` contains a single-sided copper layout (SVG) for laser/etched PCBs.

## Build

PlatformIO:

```bash
pio run -e esp32dev -t upload      # firmware
pio test -e native                 # protocol + client unit tests, no hardware needed
```

Set `WIFI_SSID` / `WIFI_PASS` in `src/main.cpp` first.

## Design

The protocol stack is layered so it can be tested on a PC:

- `IByteChannel`: byte transport (UART + L9637D on hardware, in-memory in tests)
- `Protocol`: KWP1281 block framing and complement handshake
- `IBlockIo` / `KwpClient`: session logic (connect, read, keep-alive), tested with
  scripted blocks

## Scope and safety

This is a **read-only diagnostic** tool for vehicles you own or are authorised to
work on. It does not write to ECU memory, and it doesn't touch immobiliser or security
access. Don't use it while driving.

## License

MIT
