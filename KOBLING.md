# Kobling

## ST7789V TFT 2.0" 240x320

| Skjermpinne | ESP32 GPIO |
|---|---|
| CS | GPIO5 |
| DC | GPIO19 |
| RST | GPIO22 |
| SDA / MOSI | GPIO23 |
| SCL / SCK | GPIO18 |
| VCC | 3V3 |
| GND | GND |

## Viktig

- Bruk felles GND mellom ESP32 og skjerm.
- Denne skjermen fungerte med `tft.init(240, 320)`, `setRotation(1)` og `invertDisplay(true)`.
- Baklys alene betyr ikke at skjermen kommuniserer riktig. Hvis du kun får baklys, kontroller DC/RST først.
