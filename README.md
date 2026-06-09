# ESP32 Aksjeskjerm

ESP32-prosjekt for visning av aksje- og markedsdata på ST7789V 2.0" TFT-skjerm, 240x320 px i landscape-modus.

## Funksjoner

- Henter kurser fra Yahoo Finance
- Viser én aksje om gangen i hoveddisplay
- Rullerende børslinje nederst
- Grønn/rød visning for opp/ned
- Mini trendgraf basert på 1 måned historikk
- Wi-Fi-status på skjermen
- Automatisk kursoppdatering hvert 60. sekund
- Automatisk bytte av hovedaksje hvert 7. sekund

## Skjerm

Bekreftet fungerende skjermoppsett:

| TFT | ESP32 |
|---|---|
| CS | GPIO5 |
| DC | GPIO19 |
| RST | GPIO22 |
| SDA / MOSI | GPIO23 |
| SCL / SCK | GPIO18 |
| VCC | 3V3 |
| GND | GND |

## Biblioteker

Installer disse i Arduino IDE:

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- ArduinoJson

## Wi-Fi

Åpne `ESP32_Aksje_Skjerm.ino` og endre:

```cpp
const char WIFI_SSID[] = "DITT_WIFI_NAVN";
const char WIFI_PASSWORD[] = "DITT_WIFI_PASSORD";
```

Ikke legg ekte Wi-Fi-passord i offentlig GitHub-repo.

## Arduino IDE

Anbefalt kortvalg:

- Board: ESP32 Dev Module / DOIT ESP32 DEVKIT V1
- Baud: 115200
- Flash Frequency: 80 MHz
- Upload Speed: 921600 eller lavere ved opplastingsfeil

## Data

Yahoo Finance API-endepunkt som brukes:

```text
https://query1.finance.yahoo.com/v8/finance/chart/<SYMBOL>?interval=1d&range=1mo
```

Merk: Yahoo Finance er ikke garantert stabilt som offisielt API. Koden er ment for hobby-/visningsbruk.
