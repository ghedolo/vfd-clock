/*
 * clockMoveSepAnimazione.ino
 * HH:MM big-font clock on 20×2 VFD/LCD (HD44780 compatible).
 * Periodic horizontal shift to distribute pixel wear across columns.
 *
 * Layout (20 columns, centered + variable offset):
 *
 *  col: 0  1  2 | 3  4 | 5 | 6  7 | 8 | 9 | 10 | 11 12 | 13 | 14 15 | 16…19
 *                [ H1  ]sep[ H2  ]     :     [  M1   ]sep[  M2   ]
 *
 * Wiring (Arduino Nano):
 *   RS(pin4)→D12  R/W(pin5)→D11(LOW)  E(pin6)→D10
 *   D4(pin11)→D5  D5(pin12)→D4  D6(pin13)→D3  D7(pin14)→D2
 *
 * Piezo buzzer: D9 signal (OC1A Timer1), D8 software GND
 * Button: D7 input (pullup), D6 software GND
 *
 * RTC DS3231 (ZS-042 module) via I2C:
 *   SDA → A4   SCL → A5   VCC → 5V   GND → GND
 *
 * Serial commands (9600 baud): see setup() for full list.
 *
 * Copyright (C) 2026 ghedo (luca.ghedini@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Font design (TILES/DIGITS matrices): CC BY-SA 4.0
 */

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <math.h>
#include <avr/wdt.h>

RTC_DS3231 rtc;
bool rtcOk = false;

// ── GPS coordinates (EEPROM) ────────────────────────────────────────────────
#define EEPROM_MAGIC     0xA5
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_LAT   1   // float 4 byte
#define EEPROM_ADDR_LON   5   // float 4 byte

// Default: Rome
float gpsLat = 41.9028;
float gpsLon = 12.4964;

// Sunrise/sunset in minutes from midnight (local time)
int sunriseMin = 0;
int sunsetMin  = 0;
int lastSunDay = -1;  // day of year of last calculation

// Auto-dimming: current automatic brightness level
int autoBrLevel = 4;
int curBrLevel = 1;  // current brightness level (0=off..4=100%)
bool brManualOverride = false;  // true if user set brightness manually

// ── DST EU (CET/CEST) ──────────────────────────────────────────────────────

// Days in month (leap-year aware)
byte daysInMon(int year, byte month) {
  if (month == 2)
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

// Last Sunday of month: returns day (1-31)
byte lastSunday(int year, byte month) {
  byte dim = daysInMon(year, month);
  DateTime last(year, month, dim, 0, 0, 0);
  byte dow = last.dayOfTheWeek();  // 0=Sun
  return dim - dow;
}

// Returns true if UTC DateTime falls within EU daylight saving time
bool isDST(const DateTime &utc) {
  byte month = utc.month();
  if (month < 3 || month > 10) return false;   // Nov-Feb: winter
  if (month > 3 && month < 10) return true;     // Apr-Sep: summer
  byte ls = lastSunday(utc.year(), month);
  byte day = utc.day();
  byte hour = utc.hour();
  if (month == 3) {  // March: DST starts last Sunday at 02:00 UTC
    return (day > ls) || (day == ls && hour >= 2);
  }
  // October: DST ends last Sunday at 03:00 UTC
  return (day < ls) || (day == ls && hour < 3);
}

// Convert UTC to local time (CET=UTC+1, CEST=UTC+2)
void utcToLocal(const DateTime &utc, int &lh, int &lm) {
  int offset = isDST(utc) ? 2 : 1;
  lh = (utc.hour() + offset) % 24;
  lm = utc.minute();
}

// Subtract hours from a local date/time and return a DateTime
static DateTime subtractHours(int year, byte month, byte day,
                              int lh, int lm, int ls, int hours) {
  int uh = lh - hours;
  byte ud = day;
  byte um = month;
  int uy = year;
  if (uh < 0) {
    uh += 24;
    ud--;
    if (ud == 0) {
      um--;
      if (um == 0) { um = 12; uy--; }
      ud = daysInMon(uy, um);
    }
  }
  return DateTime(uy, um, ud, uh, lm, ls);
}

// Convert local time to UTC (for saving to DS3231)
void localToUtc(int year, byte month, byte day, int lh, int lm, int ls,
                DateTime &utcOut) {
  // Try CET first (UTC+1)
  DateTime trial = subtractHours(year, month, day, lh, lm, ls, 1);
  if (isDST(trial)) {
    // Recalculate with CEST (UTC+2)
    trial = subtractHours(year, month, day, lh, lm, ls, 2);
  }
  utcOut = trial;
}

// ── EEPROM GPS coordinates ──────────────────────────────────────────────────

void loadGpsFromEeprom() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
    EEPROM.get(EEPROM_ADDR_LAT, gpsLat);
    EEPROM.get(EEPROM_ADDR_LON, gpsLon);
  }
}

void saveGpsToEeprom() {
  EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.put(EEPROM_ADDR_LAT, gpsLat);
  EEPROM.put(EEPROM_ADDR_LON, gpsLon);
}

// ── Simplified sunrise/sunset calculation ───────────────────────────────────
// Based on solar declination + hour angle
// Accuracy ~10-15 minutes, sufficient for dimming

void calcSunTimes(int dayOfYear, bool dst) {
  float latRad = gpsLat * M_PI / 180.0;

  // Solar declination (simplified formula)
  float decl = -23.45 * cos(2.0 * M_PI * (dayOfYear + 10) / 365.0);
  float declRad = decl * M_PI / 180.0;

  // Hour angle at sunrise/sunset (elevation = -0.833° for refraction)
  float cosH = (sin(-0.01454) - sin(latRad) * sin(declRad))
             / (cos(latRad) * cos(declRad));

  // Clamp for extreme latitudes
  if (cosH > 1.0)  { sunriseMin = 0;    sunsetMin = 0;    return; }  // polar night
  if (cosH < -1.0) { sunriseMin = 0;    sunsetMin = 1439; return; }  // midnight sun

  float H = acos(cosH) * 180.0 / M_PI;  // degrees

  // Equation of time (correction in minutes)
  float B = 2.0 * M_PI * (dayOfYear - 81) / 365.0;
  float eqTime = 9.87 * sin(2*B) - 7.53 * cos(B) - 1.5 * sin(B);

  // Solar noon (minutes from midnight UTC)
  float solarNoon = 720.0 - 4.0 * gpsLon - eqTime;

  // Sunrise and sunset in UTC minutes
  float srUTC = solarNoon - H * 4.0;
  float ssUTC = solarNoon + H * 4.0;

  // Convert to local time
  int offset = dst ? 120 : 60;  // CET=+60min, CEST=+120min
  sunriseMin = ((int)round(srUTC) + offset + 1440) % 1440;
  sunsetMin  = ((int)round(ssUTC) + offset + 1440) % 1440;
}

// Day of year from month and day
int dayOfYear(int month, int day) {
  const int cum[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  return cum[month - 1] + day;
}

// Recalculate sunrise/sunset if day changed
void updateSunTimes() {
  if (!rtcOk) return;
  DateTime utcNow = rtc.now();
  int lh, lm;
  utcToLocal(utcNow, lh, lm);
  int doy = dayOfYear(utcNow.month(), utcNow.day());
  if (doy == lastSunDay) return;
  lastSunDay = doy;
  calcSunTimes(doy, isDST(utcNow));
}

#define ANIM_MOVE   55    // ms per animation frame
#define ANIM_BLANK 100    // ms blank pause between animations

// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 10, 5, 4, 3, 2);

// ── Font system ─────────────────────────────────────────────────────────────
// Each font: 8 tiles (8 bytes each) + 10 digits (4 bytes each)
typedef struct {
  byte tiles[8][8];
  byte digits[10][4];
} Font;

const Font FONTS[] PROGMEM = {
  // Font 0: edgy_h2v3 — horiz bars 2 row, vert bars 3 col
  {
    {
      { 0x1F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C }, // 0: A
      { 0x1F, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07 }, // 1: flip(A)
      { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x1F }, // 2: C
      { 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1F }, // 3: flip(C)
      { 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 4: F
      { 0x1F, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1F }, // 5: G
      { 0x1F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x1F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x16 },  // 1  (ROM 0x16 = right 3 col)
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x16 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 1: edgy_h3v3 — horiz bars 3 row, vert bars 3 col
  {
    {
      { 0x1F, 0x1F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C }, // 0: A
      { 0x1F, 0x1F, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x07 }, // 1: flip(A)
      { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x1F, 0x1F }, // 2: C
      { 0x07, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1F, 0x1F }, // 3: flip(C)
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 4: F
      { 0x1F, 0x1F, 0x1F, 0x07, 0x07, 0x1F, 0x1F, 0x1F }, // 5: G
      { 0x1F, 0x1F, 0x1F, 0x1C, 0x1C, 0x1F, 0x1F, 0x1F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x16 },  // 1  (ROM 0x16 = right 3 col)
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x16 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 2: edgy_h2v2 — horiz bars 2 row, vert bars 2 col
  {
    {
      { 0x1F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 }, // 0: A
      { 0x1F, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 }, // 1: flip(A)
      { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F }, // 2: C
      { 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x1F, 0x1F }, // 3: flip(C)
      { 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 4: F
      { 0x1F, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x1F, 0x1F }, // 5: G
      { 0x1F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x17 },  // 1  (ROM 0x17 = right 2 col)
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x17 },  // 7  (ROM 0x17 = right 2 col)
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 3: edgy_h3v4 — horiz bars 3 row, vert bars 4 col
  {
    {
      { 0x1F, 0x1F, 0x1F, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E }, // 0: A
      { 0x1F, 0x1F, 0x1F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F }, // 1: flip(A)
      { 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F, 0x1F, 0x1F }, // 2: C
      { 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F }, // 3: flip(C)
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 4: F
      { 0x1F, 0x1F, 0x1F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F }, // 5: G
      { 0x1F, 0x1F, 0x1F, 0x1E, 0x1E, 0x1F, 0x1F, 0x1F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x15 },  // 1  (ROM 0x15 = right 4 col)
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x15 },  // 7  (ROM 0x15 = right 4 col)
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 4: curvy_h2v3 — rounded corners, horiz 2 row, vert 3 col
  {
    {
      { 0x0F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C }, // 0: A
      { 0x1E, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07 }, // 1: flip(A)
      { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x0F }, // 2: C
      { 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1E }, // 3: flip(C)
      { 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 4: F
      { 0x1E, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1E }, // 5: G
      { 0x0F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x0F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x16 },  // 1
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x16 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 5: curvy_h3v3 — rounded corners, horiz 3 row, vert 3 col
  {
    {
      { 0x0F, 0x1F, 0x1F, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C }, // 0: A
      { 0x1E, 0x1F, 0x1F, 0x07, 0x07, 0x07, 0x07, 0x07 }, // 1: flip(A)
      { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F, 0x1F, 0x0F }, // 2: C
      { 0x07, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1F, 0x1E }, // 3: flip(C)
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 4: F
      { 0x1E, 0x1F, 0x1F, 0x07, 0x07, 0x1F, 0x1F, 0x1E }, // 5: G
      { 0x0F, 0x1F, 0x1F, 0x1C, 0x1C, 0x1F, 0x1F, 0x0F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x16 },  // 1
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x16 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 6: curvy_h2v2 — rounded corners, horiz 2 row, vert 2 col
  {
    {
      { 0x0F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 }, // 0: A
      { 0x1E, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 }, // 1: flip(A)
      { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x0F }, // 2: C
      { 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x1F, 0x1E }, // 3: flip(C)
      { 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 4: F
      { 0x1E, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x1F, 0x1E }, // 5: G
      { 0x0F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x0F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x17 },  // 1
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x17 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 7: curvy_h3v2 — rounded corners, horiz 3 row, vert 2 col
  {
    {
      { 0x0F, 0x1F, 0x1F, 0x18, 0x18, 0x18, 0x18, 0x18 }, // 0: A
      { 0x1E, 0x1F, 0x1F, 0x03, 0x03, 0x03, 0x03, 0x03 }, // 1: flip(A)
      { 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F, 0x0F }, // 2: C
      { 0x03, 0x03, 0x03, 0x03, 0x03, 0x1F, 0x1F, 0x1E }, // 3: flip(C)
      { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 4: F
      { 0x1E, 0x1F, 0x1F, 0x03, 0x03, 0x1F, 0x1F, 0x1E }, // 5: G
      { 0x0F, 0x1F, 0x1F, 0x18, 0x18, 0x1F, 0x1F, 0x0F }, // 6: flip(G)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }, // 7: M
    },
    {
      { 0,    1,    2,    3    },  // 0
      { 0x20, 1,    0x20, 0x17 },  // 1
      { 4,    5,    2,    7    },  // 2
      { 4,    5,    7,    3    },  // 3
      { 2,    3,    0x20, 3    },  // 4
      { 6,    4,    7,    3    },  // 5
      { 6,    4,    2,    3    },  // 6
      { 0,    1,    0x20, 0x17 },  // 7
      { 6,    5,    2,    3    },  // 8
      { 6,    5,    7,    3    },  // 9
    }
  },
  // Font 8: alien — diagonal strokes with dot accents (rune glyphs)
  {
    {
      { 0x1F, 0x03, 0x06, 0x0C, 0x0C, 0x18, 0x10, 0x10 }, // 0: Diag-Down-Left (/ 2px with top bar)
      { 0x1F, 0x18, 0x0C, 0x06, 0x06, 0x03, 0x01, 0x01 }, // 1: Diag-Down-Right (\ 2px with top bar)
      { 0x10, 0x10, 0x18, 0x0C, 0x0C, 0x06, 0x03, 0x1F }, // 2: Diag-Up-Left (/ 2px with bottom bar)
      { 0x01, 0x01, 0x03, 0x06, 0x06, 0x0C, 0x18, 0x1F }, // 3: Diag-Up-Right (\ 2px with bottom bar)
      { 0x1F, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 4: Top-Cap (bar + dot accent)
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x1F }, // 5: Bot-Cap (dot accent + bar)
      { 0x18, 0x18, 0x18, 0x1A, 0x18, 0x18, 0x18, 0x18 }, // 6: Left-Vert-Dotted
      { 0x03, 0x03, 0x03, 0x0B, 0x03, 0x03, 0x03, 0x03 }, // 7: Right-Vert-Dotted
    },
    {
      { 0,    1,    2,    3    },  // 0  diamond
      { 0x20, 7,    0x20, 0x17 },  // 1  right vert dotted + ROM bar
      { 4,    0,    3,    5    },  // 2  Z-zigzag
      { 4,    0,    5,    2    },  // 3  angular right
      { 6,    5,    0x20, 0x17 },  // 4  left vert + bot-cap, ROM bar
      { 1,    4,    5,    2    },  // 5  S-zigzag
      { 1,    4,    3,    2    },  // 6  closed bottom
      { 4,    1,    0x20, 7    },  // 7  angular descent
      { 1,    0,    3,    2    },  // 8  hourglass
      { 1,    0,    5,    2    },  // 9  open bottom-left
    }
  },
};
#define NUM_FONTS (sizeof(FONTS) / sizeof(FONTS[0]))
#define ALIEN_FONT_IDX  (NUM_FONTS - 1)

const char FN0[] PROGMEM = "edgy_h2v3";
const char FN1[] PROGMEM = "edgy_h3v3";
const char FN2[] PROGMEM = "edgy_h2v2";
const char FN3[] PROGMEM = "edgy_h3v4";
const char FN4[] PROGMEM = "curvy_h2v3";
const char FN5[] PROGMEM = "curvy_h3v3";
const char FN6[] PROGMEM = "curvy_h2v2";
const char FN7[] PROGMEM = "curvy_h3v2";
const char FN8[] PROGMEM = "alien";
const char* const FONT_NAMES[] PROGMEM = { FN0, FN1, FN2, FN3, FN4, FN5, FN6, FN7, FN8 };

byte currentFont = 0;

// ── Column positions ─────────────────────────────────────────────────────────
const int COL_H1    =  3;
const int COL_H2    =  6;
const int COL_COLON =  9;
const int COL_M1    = 11;
const int COL_M2    = 14;

// ── Clock state ──────────────────────────────────────────────────────────────
bool  clockRunning = false;
int   hh = 0, mm = 0;
int   last_h1 = -1, last_h2 = -1;
int   last_m1 = -1, last_m2 = -1;
int   colonPhase = 0;        // separator animation phase (0..COLON_FRAMES-1)
unsigned long lastColon = 0; // separator animation timer
int   colonCycle = 0;        // separator cycle counter within current minute
#define CYCLES_PER_MIN 15    // 15 cycles × 4s = 60s
int   colOffset = 0;         // horizontal shift (-3..+4), changes every minute

// Serial buffer (32 chars for GPS coordinates)
char serialBuf[32];
int  serialIdx = 0;
enum SerialState { SER_IDLE, SER_PREFIX_S, SER_PREFIX_P, SER_SET, SER_POS };
SerialState serState = SER_IDLE;

// RTC low battery indicator
bool battLow = false;
unsigned long lastBattBlink = 0;
bool battVisible = false;

// Piezo buzzer: D9 signal, D8 software GND
#define BUZZ_PIN  9
#define BUZZ_GND  8

// Button: D6 software GND, D7 input with pullup
#define BTN_GND   6
#define BTN_PIN   7

// ── Timer1 tone engine (OC1A = D9, no conflicts) ────────────────────────────

void buzzOn(unsigned int freq) {
  TCCR1A = (1 << COM1A0);                    // toggle OC1A on compare match
  TCCR1B = (1 << WGM12) | (1 << CS11);       // CTC, prescaler 8
  OCR1A  = (F_CPU / (2UL * 8 * freq)) - 1;
  TCNT1  = 0;
}

void buzzOff() {
  TCCR1A = 0;
  TCCR1B = 0;
  PORTB &= ~(1 << 1);  // D9 LOW
}

// Short pause with breathing LED active
byte animSpeed = 1;  // 1 = normal, 2 = double speed

void buzzWait(unsigned int ms) {
  unsigned long t = millis();
  unsigned int actual = ms / animSpeed;
  while (millis() - t < actual) { updateBreathing(); }
}

// Cricket chirp: 3 quick chirps at ~4.5 kHz
// Silent between 21:00 and 07:59
void clickMinute() {
  if (hh >= 21 || hh < 8) return;
  for (byte i = 0; i < 3; i++) {
    buzzOn(4500);
    buzzWait(30);
    buzzOff();
    buzzWait(25);
  }
}

// Breathing LED (pin 13, software PWM)
#define BREATH_PIN      13
#define BREATH_PERIOD   4000   // breath duration (rise+fall) in ms
#define BREATH_PAUSE    3000   // pause between breaths in ms
#define BREATH_TOTAL    (BREATH_PERIOD + BREATH_PAUSE)
#define BREATH_PWM_US   10000  // software PWM period in microseconds
unsigned long breathStart = 0;

// CR2032 battery voltage reading on A3
#define BATT_PIN A3
#define BATT_LOW_MV 2500  // threshold in millivolts

// ── Display functions ────────────────────────────────────────────────────────

// Helper: read byte from current font's DIGITS matrix in PROGMEM
inline byte dg(int d, int i) { return pgm_read_byte(&FONTS[currentFont].digits[d][i]); }

// Load current font tiles into CGRAM
void loadFont() {
  byte tmp[8];
  for (int t = 0; t < 8; t++) {
    memcpy_P(tmp, FONTS[currentFont].tiles[t], 8);
    lcd.createChar(t, tmp);
  }
}

void printBigDigit(int d, int col) {
  lcd.setCursor(col, 0);
  lcd.write(dg(d,0));
  lcd.write(dg(d,1));
  lcd.setCursor(col, 1);
  lcd.write(dg(d,2));
  lcd.write(dg(d,3));
}

// ── Separator animation (19 steps) ──────────────────────────────────────────
//  ◇ = 0x97 (empty lozenge), ◆ = 0x96 (filled lozenge), ' ' = space
#define COLON_FRAMES 19
const byte colonAnim[COLON_FRAMES][2] PROGMEM = {
  { ' ',  ' '  },  // 1
  { 0x97, ' '  },  // 2
  { ' ',  0x97 },  // 3
  { ' ',  0x96 },  // 4
  { 0x96, 0x97 },  // 5
  { 0x96, 0x96 },  // 6
  { 0x97, 0x96 },  // 7
  { 0x96, 0x97 },  // 8
  { 0x97, ' '  },  // 9
  { ' ',  0x97 },  // 3
  { ' ',  0x96 },  // 4
  { 0x96, 0x97 },  // 5
  { 0x96, 0x96 },  // 6
  { 0x97, 0x96 },  // 7
  { 0x96, 0x97 },  // 8
  { 0x96, 0x96 },  // 6
  { 0x97, 0x96 },  // 7
  { ' ',  0x96 },  // 4
  { ' ',  0x97 },  // 3
};
// Duration of each step in ms (step 1 = long pause)
const unsigned int colonDur[COLON_FRAMES] PROGMEM = {
  2419, 50, 57, 64, 71, 168, 86, 93, 90, 95, 90, 85, 80, 75, 70, 242, 60, 55, 50
};

byte alienSep(byte ch) {
  if (currentFont == ALIEN_FONT_IDX) {
    if (ch == 0x96) return 0xDC;  // filled lozenge → Futaba ROM 0xDC
    if (ch == 0x97) return 0xCC;  // empty lozenge  → Futaba ROM 0xCC
  }
  return ch;
}

void drawColon() {
  int cc = COL_COLON + colOffset;
  if (cc >= 0 && cc <= 19) {
    lcd.setCursor(cc, 0); lcd.write(alienSep(pgm_read_byte(&colonAnim[colonPhase][0])));
    lcd.setCursor(cc, 1); lcd.write(alienSep(pgm_read_byte(&colonAnim[colonPhase][1])));
  }
}

// ── Primitive helpers (with bounds check) ────────────────────────────────────

void wc(int col, int row, byte ch) {
  if (col >= 0 && col <= 19) { lcd.setCursor(col, row); lcd.write(ch); }
}
void wSP(int col)         { wc(col,0,' ');       wc(col,1,' '); }
void wDL(int col, int d)  { wc(col,0,dg(d,0));  wc(col,1,dg(d,2)); }
void wDR(int col, int d)  { wc(col,0,dg(d,1));  wc(col,1,dg(d,3)); }
void wD (int col, int d)  { wDL(col,d); wDR(col+1,d); }

void animateDigits(int h1, int h2, int m1, int m2) {
  // Base positions with global offset
  int bH1 = COL_H1 + colOffset;
  int bH2 = COL_H2 + colOffset;
  int bM1 = COL_M1 + colOffset;
  int bM2 = COL_M2 + colOffset;

  // Hours: H1 shifts +1 toward center when H2=1
  int ocol_h1 = bH1 + (last_h2==1 ? 1 : 0);
  int ncol_h1 = bH1 + (h2==1     ? 1 : 0);
  // Minutes: M1 shifts -1 when M1=1; M2 shifts -1 for M2=1 and another -1 for M1=1
  int ocol_m1 = bM1 - (last_m1==1 ? 1 : 0);
  int ncol_m1 = bM1 - (m1==1     ? 1 : 0);
  int ocol_m2 = bM2 - (last_m2==1 ? 1 : 0) - (last_m1==1 ? 1 : 0);
  int ncol_m2 = bM2 - (m2==1     ? 1 : 0) - (m1==1     ? 1 : 0);

  bool ch1 = (h1!=last_h1) || (ncol_h1!=ocol_h1);
  bool ch2 = (h2!=last_h2);
  bool cm1 = (m1!=last_m1);
  bool cm2 = (m2!=last_m2) || (ncol_m2!=ocol_m2);
  if (!(ch1||ch2||cm1||cm2)) return;

  int oh1=last_h1, oh2=last_h2, om1=last_m1, om2=last_m2;

  // ── EXIT: 3 steps to the right ─────────────────────────────────────────
  for (int k = 1; k <= 3; k++) {
    if (ch1) { wSP(ocol_h1+k-1); wD(ocol_h1+k, oh1); }
    if (ch2) { wSP(bH2+k-1);     wD(bH2+k,     oh2); }
    if (cm1) { wSP(ocol_m1+k-1); wD(ocol_m1+k, om1); }
    if (cm2) { wSP(ocol_m2+k-1); wD(ocol_m2+k, om2); }
    buzzWait(ANIM_MOVE);
  }
  // cleanup: digit occupied col+3 and col+4 in last step
  if (ch1) { wSP(ocol_h1+3); wSP(ocol_h1+4); }
  if (ch2) { wSP(bH2+3);     wSP(bH2+4);     }
  if (cm1) { wSP(ocol_m1+3); wSP(ocol_m1+4); }
  if (cm2) { wSP(ocol_m2+3); wSP(ocol_m2+4); }
  buzzWait(ANIM_BLANK);

  // ── ENTER: 3 steps from the left ───────────────────────────────────────────
  for (int k = 3; k >= 1; k--) {
    if (k < 3) {  // clear column freed by previous step
      if (ch1) wSP(ncol_h1-k-1);
      if (ch2) wSP(bH2-k-1);
      if (cm1) wSP(ncol_m1-k-1);
      if (cm2) wSP(ncol_m2-k-1);
    }
    if (ch1) wD(ncol_h1-k, h1);
    if (ch2) wD(bH2-k,     h2);
    if (cm1) wD(ncol_m1-k, m1);
    if (cm2) wD(ncol_m2-k, m2);
    buzzWait(ANIM_MOVE);
  }
  // final position + cleanup col-1 (skip if TL=space: nothing to clean, risk of erasing neighbour)
  if (ch1) { if(dg(h1,0)!=0x20) wSP(ncol_h1-1); wD(ncol_h1, h1); }
  if (ch2) { if(dg(h2,0)!=0x20) wSP(bH2-1);     wD(bH2,     h2); }
  if (cm1) { if(dg(m1,0)!=0x20) wSP(ncol_m1-1); wD(ncol_m1, m1); }
  if (cm2) { if(dg(m2,0)!=0x20) wSP(ncol_m2-1); wD(ncol_m2, m2); }

  last_h1=h1; last_h2=h2; last_m1=m1; last_m2=m2;

  // restore static digits possibly overwritten during transit
  if (!ch1) printBigDigit(h1, ncol_h1);
  if (!ch2) printBigDigit(h2, bH2);
  if (!cm1) printBigDigit(m1, ncol_m1);
  if (!cm2) printBigDigit(m2, ncol_m2);
  drawColon();
}

void drawWaiting() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  Initial time: ");
  lcd.setCursor(0, 1); lcd.print("   HHMM + enter ");
}

// ── Redraw all digits (after offset change or startup) ───────────────────────

void redrawAll() {
  int h2v = hh % 10, m1v = mm / 10, m2v = mm % 10;
  lcd.clear();
  printBigDigit(hh/10, COL_H1 + colOffset + (h2v==1 ? 1 : 0));
  printBigDigit(h2v,   COL_H2 + colOffset);
  printBigDigit(m1v,   COL_M1 + colOffset - (m1v==1 ? 1 : 0));
  printBigDigit(m2v,   COL_M2 + colOffset - (m2v==1 ? 1 : 0) - (m1v==1 ? 1 : 0));
  drawColon();
  last_h1 = hh / 10;  last_h2 = h2v;
  last_m1 = m1v;      last_m2 = m2v;
}

// ── Update offset; returns true if changed ───────────────────────────────────

bool updateOffset() {
  int newOff = (mm % 8) - 3;
  if (newOff == colOffset) return false;
  colOffset = newOff;
  redrawAll();
  return true;
}

// ── CR2032 battery voltage reading ───────────────────────────────────────────

int readBattMv() {
  int raw = analogRead(BATT_PIN);
  return (int)((long)raw * 5000 / 1023);
}

void checkBatt() {
  int mv = readBattMv();
  Serial.print(F("Batt: "));
  Serial.print(mv / 1000); Serial.print('.'); Serial.print((mv % 1000) / 100); Serial.println(F("V"));
  if (mv < BATT_LOW_MV && mv > 100) {
    battLow = true;
    lastBattBlink = millis();
  }
}

// ── Breathing LED (software PWM on pin 13) ──────────────────────────────────

// Direct port manipulation for pin 13 (PB5) — ~50× faster than digitalWrite
#define BREATH_ON()   (PORTB |=  (1 << 5))
#define BREATH_OFF()  (PORTB &= ~(1 << 5))

void updateBreathing() {
  unsigned long now = millis();
  unsigned long cycle = (now - breathStart) % BREATH_TOTAL;

  // During pause: LED off
  if (cycle >= BREATH_PERIOD) {
    BREATH_OFF();
    return;
  }

  // Cosine ramp: smooth ease-in and ease-out, no snap at peak
  // Maps cycle 0→BREATH_PERIOD to brightness 0→255→0 via raised cosine
  float phase = (float)cycle / BREATH_PERIOD;          // 0.0 → 1.0
  int brightness = (int)(127.5 * (1.0 - cos(2.0 * M_PI * phase)));

  // Software PWM: compare duty cycle with position in micro-cycle
  unsigned long pwmPos = micros() % BREATH_PWM_US;
  unsigned long onTime = (unsigned long)brightness * BREATH_PWM_US / 255;
  if (pwmPos < onTime) BREATH_ON(); else BREATH_OFF();
}

// ── Sync RTC with current local time ─────────────────────────────────────────

void syncRTC() {
  if (!rtcOk) return;
  // Read current date from DS3231, update hours and minutes only
  DateTime now = rtc.now();
  DateTime utcOut;
  localToUtc(now.year(), now.month(), now.day(), hh, mm, 0, utcOut);
  rtc.adjust(utcOut);
}

// ── Start clock ──────────────────────────────────────────────────────────────

void startClock(int h, int m, int sec = 0) {
  hh = h;  mm = m;
  last_h1 = last_h2 = last_m1 = last_m2 = -1;
  colonPhase = 0;
  // Skip cycles already elapsed within the current minute
  unsigned long elapsedMs = (unsigned long)sec * 1000UL;
  colonCycle = elapsedMs / 4000UL;
  clockRunning = true;
  colOffset = (mm % 8) - 3;
  redrawAll();
  // Backdate timer by the remainder so the next cycle fires at the right time
  lastColon = millis() - (elapsedMs % 4000UL);
}

// ── Adjust clock by signed delta minutes ─────────────────────────────────────

void adjustMinutes(int delta) {
  int total = ((hh * 60 + mm + delta) % 1440 + 1440) % 1440;
  hh = total / 60;  mm = total % 60;
  animateDigits(hh / 10, hh % 10, mm / 10, mm % 10);
  updateOffset();
  syncRTC();
  colonCycle = 0;  colonPhase = 0;  lastColon = millis();
  Serial.print(delta > 0 ? F("+") : F(""));
  Serial.print(delta); Serial.print(F("min -> "));
  if (hh < 10) Serial.print('0'); Serial.print(hh);
  Serial.print(':');
  if (mm < 10) Serial.print('0'); Serial.println(mm);
}

// ── VFD brightness ───────────────────────────────────────────────────────────
// 0=display off, 1=25%, 2=50%, 3=75%, 4=100%
// Function Set: 0b00101_BR1_BR0  (4-bit, 2 lines, N=1)
// BR1,BR0: 00=100%, 01=75%, 10=50%, 11=25%
const byte brMap[] PROGMEM = { 0, 0b11, 0b10, 0b01, 0b00 };
bool brSilent = false;  // true during boot animations to silence Serial

void setBrightness(int level) {
  curBrLevel = level;
  if (level == 0) {
    lcd.command(0x08);
    if (!brSilent) Serial.println(F("Display OFF"));
  } else {
    lcd.command(0x0C);
    lcd.command(0x28 | pgm_read_byte(&brMap[level]));
    if (!brSilent) {
      Serial.print(F("Brightness: "));
      Serial.print(level * 25);
      Serial.println(F("%"));
    }
  }
}

// ── Auto-dimming ─────────────────────────────────────────────────────────────

int calcAutoBrightness() {
  int nowMin = hh * 60 + mm;
  int srEarly = sunriseMin - 30;
  int srLate  = sunriseMin + 30;
  int ssEarly = sunsetMin  - 30;
  int ssLate  = sunsetMin  + 30;

  // Normalize negative values
  if (srEarly < 0) srEarly += 1440;

  // Full day: sunrise+30 .. sunset-30
  if (nowMin >= srLate && nowMin <= ssEarly) return 4;   // 100%
  // Sunrise transition: sunrise-30 .. sunrise+30
  if (nowMin >= srEarly && nowMin < srLate)  return 2;   // 50%
  // Sunset transition: sunset-30 .. sunset+30
  if (nowMin > ssEarly && nowMin <= ssLate)  return 2;   // 50%
  // Night
  return 1;  // 25%
}

// force=true when '9' is pressed to force immediate application
void applyAutoBrightness(bool force = false) {
  if (brManualOverride) return;
  int newLevel = calcAutoBrightness();
  if (newLevel != autoBrLevel || force) {
    autoBrLevel = newLevel;
    setBrightness(autoBrLevel);
  }
}

// ── Serial handling ──────────────────────────────────────────────────────────

// Parse and apply the s:DDMMYYYY-HHmmSS command (local time)
void handleSetCommand() {
  // serialBuf contains "DDMMYYYY-HHmmSS" (15 chars)
  if (serialIdx != 15 || serialBuf[8] != '-') {
    Serial.println(F("ERR: format s:DDMMYYYY-HHmmSS"));
    return;
  }
  int dy = (serialBuf[0]-'0')*10 + (serialBuf[1]-'0');
  int mo = (serialBuf[2]-'0')*10 + (serialBuf[3]-'0');
  int yr = (serialBuf[4]-'0')*1000 + (serialBuf[5]-'0')*100
         + (serialBuf[6]-'0')*10   + (serialBuf[7]-'0');
  int hr = (serialBuf[9]-'0')*10  + (serialBuf[10]-'0');
  int mi = (serialBuf[11]-'0')*10 + (serialBuf[12]-'0');
  int se = (serialBuf[13]-'0')*10 + (serialBuf[14]-'0');

  if (mo<1||mo>12||dy<1||dy>31||hr>23||mi>59||se>59) {
    Serial.println(F("ERR: values out of range"));
    return;
  }

  if (rtcOk) {
    DateTime utcOut;
    localToUtc(yr, mo, dy, hr, mi, se, utcOut);
    rtc.adjust(utcOut);
    Serial.print(F("RTC UTC -> "));
    if (utcOut.hour()<10) Serial.print('0'); Serial.print(utcOut.hour());
    Serial.print(':');
    if (utcOut.minute()<10) Serial.print('0'); Serial.println(utcOut.minute());
  }
  startClock(hr, mi, se);
  Serial.print(F("OK local "));
  if (dy<10) Serial.print('0'); Serial.print(dy);
  Serial.print('/');
  if (mo<10) Serial.print('0'); Serial.print(mo);
  Serial.print('/'); Serial.print(yr);
  Serial.print(' ');
  if (hr<10) Serial.print('0'); Serial.print(hr);
  Serial.print(':');
  if (mi<10) Serial.print('0'); Serial.print(mi);
  Serial.print(':');
  if (se<10) Serial.print('0'); Serial.println(se);
  bool dst = false;
  if (rtcOk) { dst = isDST(rtc.now()); }
  Serial.print(F("DST: ")); Serial.println(dst ? F("YES (CEST UTC+2)") : F("NO (CET UTC+1)"));
  if (battLow) {
    battLow = false;
    lcd.setCursor(0, 0); lcd.write(' ');
  }
}

// Parse and apply the p:lat,lon command
void handlePosCommand() {
  serialBuf[serialIdx] = '\0';
  // Find comma separator (Google Maps uses ", ")
  char *sep = NULL;
  for (int i = 0; i < serialIdx; i++) {
    if (serialBuf[i] == ',') { sep = &serialBuf[i]; break; }
  }
  if (!sep) {
    Serial.println(F("ERR: format p:lat,lon"));
    return;
  }
  *sep = '\0';
  // Skip spaces after comma
  char *lonStr = sep + 1;
  while (*lonStr == ' ') lonStr++;

  float lat = atof(serialBuf);
  float lon = atof(lonStr);

  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    Serial.println(F("ERR: coordinates out of range"));
    return;
  }

  gpsLat = lat;
  gpsLon = lon;
  saveGpsToEeprom();

  Serial.print(F("Pos: ")); Serial.print(gpsLat, 4);
  Serial.print(F(", ")); Serial.println(gpsLon, 4);

  // Recalculate sunrise/sunset with new coordinates
  lastSunDay = -1;
  updateSunTimes();
  brManualOverride = false;
  applyAutoBrightness(true);

  Serial.print(F("Sunrise: "));
  Serial.print(sunriseMin / 60); Serial.print(':');
  if (sunriseMin % 60 < 10) Serial.print('0'); Serial.print(sunriseMin % 60);
  Serial.print(F("  Sunset: "));
  Serial.print(sunsetMin / 60); Serial.print(':');
  if (sunsetMin % 60 < 10) Serial.print('0'); Serial.println(sunsetMin % 60);
}

void handleSerialChar(char c) {
  if (c == '\r') return;
  // Echo character (except newline)
  if (c != '\n') Serial.write(c);
  else Serial.println();

  // Collecting data for s: or p: command
  if (serState == SER_SET) {
    if (c == '\n') {
      handleSetCommand();
      serState = SER_IDLE;
      serialIdx = 0;
    } else if (serialIdx < 15) {
      serialBuf[serialIdx++] = c;
    }
    return;
  }
  if (serState == SER_POS) {
    if (c == '\n') {
      handlePosCommand();
      serState = SER_IDLE;
      serialIdx = 0;
    } else if (serialIdx < 30) {
      serialBuf[serialIdx++] = c;
    }
    return;
  }

  // Waiting for ':' after 's' or 'p'
  if (serState == SER_PREFIX_S || serState == SER_PREFIX_P) {
    if (c == ':') {
      serState = (serState == SER_PREFIX_S) ? SER_SET : SER_POS;
      serialIdx = 0;
    } else {
      serState = SER_IDLE;
    }
    return;
  }

  // Clock commands
  if (c == 'M' && clockRunning) { adjustMinutes(1);   return; }
  if (c == 'm' && clockRunning) { adjustMinutes(-1);  return; }
  if (c == 'H' && clockRunning) { adjustMinutes(60);  return; }
  if (c == 'h' && clockRunning) { adjustMinutes(-60); return; }
  if (c == 'D' && clockRunning) { adjustMinutes(10);  return; }
  if (c == 'd' && clockRunning) { adjustMinutes(-10); return; }
  // Brightness: 0=off, 1=25%, 2=50%, 3=75%, 4=100%, 9=auto
  if (c >= '0' && c <= '4') { brManualOverride = true; setBrightness(c - '0'); return; }
  if (c == '9') { brManualOverride = false; applyAutoBrightness(true); Serial.println(F("Auto-dimming")); return; }
  // Replay boot animation
  if (c == 'i') {
    brSilent = true;
    bootAnimation();
    brSilent = false;
    loadFont();
    applyAutoBrightness(true);
    if (clockRunning) redrawAll();
    else drawWaiting();
    return;
  }
  // Overall status on one line
  if (c == 'o') {
    DateTime utc = rtcOk ? rtc.now() : DateTime((uint32_t)0);
    bool dst = rtcOk ? isDST(utc) : false;
    int off = dst ? 2 : 1;
    DateTime loc = DateTime(utc.unixtime() + (uint32_t)off * 3600UL);
    int mv = readBattMv();
    char fbuf[16];
    strcpy_P(fbuf, (char*)pgm_read_ptr(&FONT_NAMES[currentFont]));
    char line[80];
    snprintf(line, sizeof(line),
             "%04u-%02u-%02u %02u:%02u:%02u DST:%s BR:%d%% BATT:%d.%02dV FONT:%s",
             loc.year(), loc.month(), loc.day(),
             loc.hour(), loc.minute(), loc.second(),
             dst ? "YES" : "NO",
             curBrLevel * 25,
             mv / 1000, (mv % 1000) / 10,
             fbuf);
    Serial.println(line);
    return;
  }
  // CR2032 battery voltage
  if (c == 'v') {
    int mv = readBattMv();
    Serial.print(F("Battery: "));
    Serial.print(mv / 1000); Serial.print('.'); Serial.print((mv % 1000) / 100); Serial.println(F("V"));
    return;
  }
  // Software reset
  if (c == 'r') {
    lcd.command(0x08);  // display off
    lcd.clear();
    Serial.println(F("Reset..."));
    buzzWait(3000);
    wdt_enable(WDTO_15MS);
    while (true) {}
  }
  // Start set/position command prefix
  if (c == 's') { serState = SER_PREFIX_S; return; }
  if (c == 'p') { serState = SER_PREFIX_P; return; }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

// ── Boot Animation: DotDot ───────────────────────────────────────────────────

void bootAnimation() {
  // 8 empty CGRAM tiles
  byte bt[8][8];
  byte sc[2][20];

  for (int t = 0; t < 8; t++) {
    for (int r = 0; r < 8; r++) bt[t][r] = 0x00;
    lcd.createChar(t, bt[t]);
  }

  // Fill display with random tiles
  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 20; c++) {
      sc[r][c] = random(8);
      lcd.setCursor(c, r);
      lcd.write((byte)sc[r][c]);
    }
  }

  setBrightness(1);

  // Growth phase: 20 iterations, 4 tiles × 8 random pixels (20% faster)
  for (int iter = 0; iter < 20; iter++) {
    int br = 1 + (iter * 3) / 14;
    if (br > 4) br = 4;
    setBrightness(br);

    for (int ti = 0; ti < 4; ti++) {
      int t = random(8);
      for (int p = 0; p < 8; p++) {
        bt[t][random(8)] |= (0x10 >> random(5));
      }
      lcd.createChar(t, bt[t]);
    }

    for (int r = 0; r < 2; r++) {
      for (int c = 0; c < 20; c++) {
        lcd.setCursor(c, r);
        lcd.write((byte)sc[r][c]);
      }
    }
    buzzWait(77);  // era 96
  }

  buzzWait(192);  // era 240

  // Fade out + split from center — row 1 starts 2 steps before row 0
  #define STAGGER 2          // bottom row head-start steps
  #define SPLIT_STEPS (10 + STAGGER)  // 12 total steps
  const byte fadeBr[] = { 4, 4, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1 };
  // Current inner edges per row: left moves toward 0, right toward 19
  int edgeL[2] = { 9, 9 };   // left edge (last visible col on left)
  int edgeR[2] = { 10, 10 }; // right edge (first visible col on right)

  for (int step = 0; step < SPLIT_STEPS; step++) {
    if (step < 9) setBrightness(fadeBr[step]);

    // Turn off pixels in 2 random tiles
    for (int ti = 0; ti < 2; ti++) {
      int t = random(8);
      for (int p = 0; p < 6; p++) {
        bt[t][random(8)] &= ~(0x10 >> random(5));
      }
      lcd.createChar(t, bt[t]);
    }

    // Redraw only still-visible columns for each row
    for (int r = 0; r < 2; r++) {
      for (int c = 0; c < 20; c++) {
        lcd.setCursor(c, r);
        lcd.write((byte)sc[r][c]);
      }
    }

    // Row 1 (bottom) moves from step 0; row 0 (top) from step STAGGER
    for (int r = 1; r >= 0; r--) {
      int startAt = (r == 1) ? 0 : STAGGER;
      if (step < startAt) continue;
      if (edgeL[r] < 0) continue;  // row already fully empty

      // Left half: shift left by 1
      for (int c = 0; c < edgeL[r]; c++) {
        sc[r][c] = sc[r][c + 1];
        lcd.setCursor(c, r); lcd.write((byte)sc[r][c]);
      }
      lcd.setCursor(edgeL[r], r); lcd.write(' ');
      sc[r][edgeL[r]] = 0x20;
      edgeL[r]--;

      // Right half: shift right by 1
      for (int c = 19; c > edgeR[r]; c--) {
        sc[r][c] = sc[r][c - 1];
        lcd.setCursor(c, r); lcd.write((byte)sc[r][c]);
      }
      lcd.setCursor(edgeR[r], r); lcd.write(' ');
      sc[r][edgeR[r]] = 0x20;
      edgeR[r]++;
    }

    buzzWait(64);  // era 80
  }

  setBrightness(0);
  buzzWait(128);  // era 160
  lcd.clear();
  setBrightness(1);
}

// ── Boot Animation Reverse: everything in reverse ───────────────────────────

void bootAnimationReverse() {
  byte bt[8][8];
  byte sc[2][20];

  // Empty screen, low brightness
  lcd.clear();
  setBrightness(1);

  // Phase 1: merge from edges toward center (reverse of split)
  // Start with empty screen and empty tiles
  for (int t = 0; t < 8; t++) {
    for (int r = 0; r < 8; r++) bt[t][r] = 0x00;
    lcd.createChar(t, bt[t]);
  }
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 20; c++)
      sc[r][c] = 0x20;  // space

  #define REV_STAGGER 2
  #define REV_SPLIT_STEPS (10 + REV_STAGGER)
  const byte revFadeBr[] = { 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4 };

  // Edges start from extremes and converge toward center
  int edgeL[2] = { -1, -1 };   // last empty col on left (grows toward 9)
  int edgeR[2] = { 20, 20 };   // first empty col on right (shrinks toward 10)

  for (int step = 0; step < REV_SPLIT_STEPS; step++) {
    if (step < 12) setBrightness(revFadeBr[step]);

    // Add pixels to 2 random tiles (light growth during merge)
    for (int ti = 0; ti < 2; ti++) {
      int t = random(8);
      for (int p = 0; p < 6; p++) {
        bt[t][random(8)] |= (0x10 >> random(5));
      }
      lcd.createChar(t, bt[t]);
    }

    // Row 0 (top) moves from step 0; row 1 from step REV_STAGGER
    for (int r = 0; r <= 1; r++) {
      int startAt = (r == 0) ? 0 : REV_STAGGER;
      if (step < startAt) continue;
      if (edgeL[r] >= 9) continue;  // row already complete

      edgeL[r]++;
      edgeR[r]--;

      // Left half: shift right by 1 (reverse of left shift)
      for (int c = edgeL[r]; c > 0; c--) {
        sc[r][c] = sc[r][c - 1];
      }
      sc[r][0] = random(8);
      for (int c = 0; c <= edgeL[r]; c++) {
        lcd.setCursor(c, r); lcd.write((byte)sc[r][c]);
      }

      // Right half: shift left by 1 (reverse of right shift)
      for (int c = edgeR[r]; c < 19; c++) {
        sc[r][c] = sc[r][c + 1];
      }
      sc[r][19] = random(8);
      for (int c = edgeR[r]; c < 20; c++) {
        lcd.setCursor(c, r); lcd.write((byte)sc[r][c]);
      }
    }

    buzzWait(64);
  }

  // Ensure entire screen is filled with tiles
  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 20; c++) {
      if (sc[r][c] == 0x20) sc[r][c] = random(8);
      lcd.setCursor(c, r);
      lcd.write((byte)sc[r][c]);
    }
  }

  buzzWait(192);

  // Phase 2: decay (reverse of growth) — pixels disappear
  setBrightness(4);
  for (int iter = 0; iter < 20; iter++) {
    int br = 4 - (iter * 3) / 14;
    if (br < 1) br = 1;
    setBrightness(br);

    for (int ti = 0; ti < 4; ti++) {
      int t = random(8);
      for (int p = 0; p < 8; p++) {
        bt[t][random(8)] &= ~(0x10 >> random(5));
      }
      lcd.createChar(t, bt[t]);
    }

    for (int r = 0; r < 2; r++) {
      for (int c = 0; c < 20; c++) {
        lcd.setCursor(c, r);
        lcd.write((byte)sc[r][c]);
      }
    }
    buzzWait(77);
  }

  setBrightness(0);
  buzzWait(128);
  lcd.clear();
  setBrightness(1);
}

// ── Button: single intro + next font ────────────────────────────────────────

void switchToFont(byte idx) {
  currentFont = idx;
  char buf[16];
  strcpy_P(buf, (char*)pgm_read_ptr(&FONT_NAMES[currentFont]));
  Serial.print(F("Font: "));
  Serial.println(buf);

  brSilent = true;
  animSpeed = 2;
  bootAnimation();
  animSpeed = 1;
  brSilent = false;

  loadFont();
  applyAutoBrightness(true);
  if (clockRunning) redrawAll();
  else drawWaiting();
}

#define DOUBLE_PRESS_MS 500

void buttonAction() {
  byte next = (currentFont + 1) % NUM_FONTS;
  if (next == ALIEN_FONT_IDX) next = 0;  // skip alien in normal carousel
  switchToFont(next);
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  pinMode(11, OUTPUT);
  digitalWrite(11, LOW);

  // Buzzer: D8 software GND, D9 signal (series resistor for volume)
  pinMode(BUZZ_GND, OUTPUT);
  digitalWrite(BUZZ_GND, LOW);
  pinMode(BUZZ_PIN, OUTPUT);

  // Button: D6 software GND, D7 input with internal pullup
  pinMode(BTN_GND, OUTPUT);
  digitalWrite(BTN_GND, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);

  lcd.begin(20, 2);
  setBrightness(1);  // 25% at startup

  pinMode(BREATH_PIN, OUTPUT);
  breathStart = millis();

  Serial.begin(9600);
  Serial.println(F("clockMoveSepAnimazione"));
  Serial.print  (F("Build: ")); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
  Serial.println();
  Serial.println(F("--- ghedo 2026 ---"));
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  s:DDMMYYYY-HHmmSS  Set local date and time"));
  Serial.println(F("  M/m                +/- 1 minute  (updates RTC)"));
  Serial.println(F("  D/d                +/- 10 minutes (updates RTC)"));
  Serial.println(F("  H/h                +/- 1 hour    (updates RTC)"));
  Serial.println(F("  0-4                Brightness (0=off 1=25% 2=50% 3=75% 4=100%)"));
  Serial.println(F("  9                  Auto-dimming (sunrise/sunset)"));
  Serial.println(F("  p:lat,lon          GPS position (e.g. p:41.9028,12.4964)"));
  Serial.println(F("  v                  CR2032 battery voltage"));
  Serial.println(F("  o                  Overall status (one line)"));
  Serial.println(F("  i                  Replay boot animation"));
  Serial.println(F("  r                  Software reset"));
  Serial.println();

  randomSeed(analogRead(A0));
  brSilent = true;
  bootAnimation();
  brSilent = false;

  loadFont();

  loadGpsFromEeprom();
  Serial.print(F("Pos: ")); Serial.print(gpsLat, 4);
  Serial.print(F(", ")); Serial.println(gpsLon, 4);

  Wire.begin();
  if (rtc.begin()) {
    rtcOk = true;
    DateTime utcNow = rtc.now();
    int lh, lm;
    utcToLocal(utcNow, lh, lm);
    bool dst = isDST(utcNow);
    int mv = readBattMv();
    Serial.print(F("RTC UTC: "));
    if (utcNow.hour() < 10) Serial.print('0'); Serial.print(utcNow.hour());
    Serial.print(':');
    if (utcNow.minute() < 10) Serial.print('0'); Serial.print(utcNow.minute());
    Serial.print(F("  DST: ")); Serial.print(dst ? F("YES") : F("NO"));
    Serial.print(F("  Local: "));
    if (lh < 10) Serial.print('0'); Serial.print(lh);
    Serial.print(':');
    if (lm < 10) Serial.print('0'); Serial.print(lm);
    Serial.print(F("  Batt: "));
    Serial.print(mv / 1000); Serial.print('.'); Serial.print((mv % 1000) / 100); Serial.println(F("V"));

    // Calculate sunrise/sunset
    updateSunTimes();
    Serial.print(F("Alba: "));
    Serial.print(sunriseMin / 60); Serial.print(':');
    if (sunriseMin % 60 < 10) Serial.print('0'); Serial.print(sunriseMin % 60);
    Serial.print(F("  Tramonto: "));
    Serial.print(sunsetMin / 60); Serial.print(':');
    if (sunsetMin % 60 < 10) Serial.print('0'); Serial.println(sunsetMin % 60);

    if (rtc.lostPower()) {
      battLow = true;
      lastBattBlink = millis();
      Serial.println(F("WARNING: RTC battery low! Time may be inaccurate."));
    }
    if (mv < BATT_LOW_MV && mv > 100) { battLow = true; lastBattBlink = millis(); }
    startClock(lh, lm, utcNow.second());
    applyAutoBrightness(true);
  } else {
    Serial.println(F("RTC not found!"));
    Serial.println(F("Use s:DDMMYYYY-HHmmSS to set date and time"));
    drawWaiting();
  }
}

// ── Cycle indicator (hex 0–D) ────────────────────────────────────────────────

void drawCycleHex() {
  const char hex[] = "0123456789ABCDEF";
  char ch = hex[colonCycle % 16];
  // Place counter on the side farthest from the clock
  int distLeft = COL_H1 + colOffset;          // free space on the left
  int distRight = 19 - (COL_M2 + 1 + colOffset); // free space on the right
  if (distLeft >= distRight) {
    lcd.setCursor(0, 1);
  } else {
    lcd.setCursor(19, 0);
  }
  lcd.write(ch);
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  while (Serial.available()) {
    handleSerialChar((char)Serial.read());
  }

  updateBreathing();  // breathing LED always active

  // Button on D7: single press = next font, double press = alien font
  if (digitalRead(BTN_PIN) == LOW) {
    delay(50);  // debounce
    if (digitalRead(BTN_PIN) == LOW) {
      // Print current date and time
      if (rtcOk) {
        DateTime utcNow = rtc.now();
        int lh, lm;
        utcToLocal(utcNow, lh, lm);
        if (utcNow.day() < 10) Serial.print('0'); Serial.print(utcNow.day());
        Serial.print('/');
        if (utcNow.month() < 10) Serial.print('0'); Serial.print(utcNow.month());
        Serial.print('/'); Serial.print(utcNow.year());
        Serial.print(' ');
        if (lh < 10) Serial.print('0'); Serial.print(lh);
        Serial.print(':');
        if (lm < 10) Serial.print('0'); Serial.println(lm);
      }
      while (digitalRead(BTN_PIN) == LOW) { updateBreathing(); }  // wait for release
      // Wait for possible second press
      unsigned long rel = millis();
      bool dbl = false;
      while (millis() - rel < DOUBLE_PRESS_MS) {
        updateBreathing();
        if (digitalRead(BTN_PIN) == LOW) {
          delay(50);
          if (digitalRead(BTN_PIN) == LOW) {
            dbl = true;
            while (digitalRead(BTN_PIN) == LOW) { updateBreathing(); }
            break;
          }
        }
      }
      if (dbl) {
        switchToFont(ALIEN_FONT_IDX);
      } else {
        buttonAction();
      }
    }
  }

  if (!clockRunning) return;

  unsigned long now = millis();

  // Low battery blink (0.3s ON, 1.2s OFF)
  if (battLow) {
    // If col 0 is occupied by the clock, use col 19; otherwise col 0
    int battCol = (COL_H1 + colOffset <= 0) ? 19 : 0;
    unsigned long elapsed = now - lastBattBlink;
    if (battVisible && elapsed >= 300) {
      lcd.setCursor(battCol, 0); lcd.write(' ');
      battVisible = false;
      lastBattBlink = now;
    } else if (!battVisible && elapsed >= 1200) {
      lcd.setCursor(battCol, 0); lcd.write((byte)0x1C);
      battVisible = true;
      lastBattBlink = now;
    }
  }

  // Separator animation (independent timer, variable duration per step)
  unsigned int curDur = pgm_read_word(&colonDur[colonPhase]);
  if (now - lastColon >= curDur) {
    lastColon += curDur;
    colonPhase = (colonPhase + 1) % COLON_FRAMES;
    // End of a full cycle?
    if (colonPhase == 0) {
      colonCycle++;
      if (colonCycle >= CYCLES_PER_MIN) {
        colonCycle = 0;
        // Read time from DS3231 (with DST) instead of incrementing with millis
        if (rtcOk) {
          DateTime utcNow = rtc.now();
          int lh, lm;
          utcToLocal(utcNow, lh, lm);
          // Drift between animation (millis, ceramic resonator) and RTC
          // (DS3231 crystal). ds > 0 = animation late, ds < 0 = early.
          int ds = (int)utcNow.second();
          if (ds > 30) ds -= 60;
          Serial.print(F("delta: "));
          Serial.print(ds);
          Serial.println(F(" s"));
          // Hard-reset lastColon to current time. This kills any
          // accumulated debt that would cause a frame burst cascade.
          lastColon = millis();
          // Then apply drift correction: clamp to ±2000ms (must stay
          // below colonDur[0] = 2419ms to avoid burst on frame 0).
          if (ds > 0) {
            unsigned long back = min((unsigned long)ds * 1000UL, 2000UL);
            lastColon -= back;
          } else if (ds < 0) {
            unsigned long fwd = min((unsigned long)(-ds) * 1000UL, 2000UL);
            lastColon += fwd;
          }
          hh = lh; mm = lm;
        } else {
          if (++mm >= 60) { mm = 0; if (++hh >= 24) hh = 0; }
        }
        clickMinute();
        updateSunTimes();
        applyAutoBrightness();
        // First animate the digits
        animateDigits(hh / 10, hh % 10, mm / 10, mm % 10);
        // Then shift the layout
        updateOffset();
      }
      // drawCycleHex();  // hex cycle counter (debug)
    }
    drawColon();
  }
}
