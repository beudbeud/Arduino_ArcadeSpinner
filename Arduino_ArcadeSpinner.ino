/*
 *  Arduino USB Arcade Spinner
 *  (C) Adrien Beudin [https://github.com/beudbeud]
 *
 *  Based on project by Alexey Melnikov [https://github.com/MiSTer-devel/Retro-Controllers-USB-MiSTer/blob/master/PaddleTwoControllersUSB/PaddleTwoControllersUSB.ino]
 *  Based on project by Mikael Norrgård <mick@daemonbite.com>
 *  Based on project [https://github.com/willoucom/Arduino_ArcadeSpinner]
 *
 *  GNU GENERAL PUBLIC LICENSE
 *  Version 3, 29 June 2007
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

///////////////// Customizable settings /////////////////////////

// Your encoder's PPR (Pulses Per Revolution)
// Common values: 300, 360, 400, 600
// Quadrature encoding multiplies this by 4 (600 PPR = 2400 counts)
#define ENCODER_PPR 600
#define ENCODER_COUNTS (ENCODER_PPR * 4)  // 2400 for 600 PPR encoder

/*
 * Game Profiles - Based on original arcade hardware PPR
 *
 * Add, remove or modify profiles as needed!
 * Just update GAME_PPR array and NUM_PROFILES will auto-calculate.
 *
 * Common arcade game PPR values:
 *   64  - Omega Race, Tac/Scan, Zektor, Star Trek (Sega standard)
 *   72  - Tempest, Super Sprint (Atari vector standard)
 *   128 - Tron, Disks of Tron
 *   144 - 720 Degrees
 *   288 - Blasteroids
 *   486 - Arkanoid, Puchi Carat (geared, very sensitive)
 *
 * Sensitivity = ENCODER_COUNTS / GAME_PPR
 * Lower PPR value = less sensitive, Higher PPR value = more sensitive
 */

// ============== CUSTOMIZE YOUR PROFILES HERE ==============
const uint16_t GAME_PPR[] = {
  64,   // Profile 0: Omega Race / Sega
  72,   // Profile 1: Tempest / Atari
  128,  // Profile 2: Tron
  288,  // Profile 3: Blasteroids
  486   // Profile 4: Arkanoid / Puchi Carat
  // Add more profiles here, e.g.:
  // 144,  // Profile 5: 720 Degrees
  // 100,  // Profile 6: Custom
};
// ==========================================================

// Default profile index (0 = first profile in GAME_PPR array)
#define DEFAULT_PROFILE 4

// Sensitivity multiplier (adjusts speed on top of PPR profile)
// 5 = slower/more precise (0.5x), 10 = normal (1.0x), 20 = faster (2.0x)
// Using integer to avoid float operations in main loop
#define SENSITIVITY_MULTIPLIER 5

// Scroll wheel sensitivity (fixed, higher = less sensitive)
#define SCROLL_SENSITIVITY 400

// Response delay in ms (lower = more responsive, 1-2ms recommended)
#define RESPONSE_DELAY 1

// Dead zone - ignores small movements (noise/vibration filtering)
// 0 = disabled, 1-3 = recommended for filtering micro-movements
#define DEAD_ZONE 1

// LED feedback duration in ms
#define LED_BLINK_TIME 80

////////////////////// Don't Touche after that //////////////////

// Encoder pins
#define PIN_A 2
#define PIN_B 3

// Button pins
#define BTN_B      5   // Left button
#define BTN_A      4   // Right button
#define BTN_START  14  // Start button
#define BTN_HOTKEY 15  // Hotkey button
#define BTN_SELECT 6   // Select button

// Pro Micro onboard LEDs
#define LED_RX 17  // RX LED pin

#define SENSITIVITY_DIVISOR 10

// Auto-calculate number of profiles from array size
#define NUM_PROFILES (sizeof(GAME_PPR) / sizeof(GAME_PPR[0]))

#include <HID-Project.h>

// Mouse button definitions
#define MOUSE_SIDE  (1 << 3)
#define MOUSE_EXTRA (1 << 4)

// Spinner state
volatile int16_t spinnerPos = 0;

// Current profile
uint8_t currentProfile = DEFAULT_PROFILE;
uint16_t currentPPR = GAME_PPR[DEFAULT_PROFILE];

// Calculate sensitivity from PPR (inline function for efficiency)
inline uint8_t getSensitivity(uint16_t ppr) {  return ENCODER_COUNTS / ppr;
}

// Clamp value to int8 range
inline int8_t clamp8(int16_t val) {
  if (val > 127) return 127;
  if (val < -127) return -127;
  return val;
}

// Blink LEDs to indicate current profile
// Blinks (profile + 1) times: Profile 0 = 1 blink, Profile 4 = 5 blinks
void blinkProfile(uint8_t profile) {
  for (uint8_t i = 0; i <= profile; i++) {
    // Turn ON both LEDs (active LOW)
    digitalWrite(LED_RX, LOW);
    TXLED1;
    delay(LED_BLINK_TIME);

    // Turn OFF both LEDs
    digitalWrite(LED_RX, HIGH);
    TXLED0;
    delay(LED_BLINK_TIME);
  }
}

// Encoder interrupt handler (optimized with direct port reading)
void encoderISR() {
  static uint8_t prev = 0;

  // Direct port reading - much faster than digitalRead()
  // PIN_A = 2 = PD1, PIN_B = 3 = PD0 on Pro Micro
  uint8_t portD = PIND;
  uint8_t a = (portD >> 1) & 1;  // PD1 (pin 2)
  uint8_t b = portD & 1;          // PD0 (pin 3)

  uint8_t curr = (b << 1) | (b ^ a);
  int8_t diff = (prev - curr) & 3;

  if (diff == 1) spinnerPos++;
  else if (diff == 3) spinnerPos--;

  prev = curr;
}

void setup() {
  // Encoder pins
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  // Button pins
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_HOTKEY, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  // LED pins
  pinMode(LED_RX, OUTPUT);
  digitalWrite(LED_RX, HIGH);  // OFF (active LOW)
  TXLED0;  // OFF

  // Attach encoder interrupts
  attachInterrupt(digitalPinToInterrupt(PIN_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_B), encoderISR, CHANGE);

  // Blink current profile on startup
  delay(500);
  blinkProfile(currentProfile);
}

void loop() {
  static bool prevBtnA = false;
  static bool prevBtnB = false;
  static int16_t prevMousePos = 0;
  static int16_t prevScrollPos = 0;

  // Read buttons
  bool hotkey = !digitalRead(BTN_HOTKEY);
  bool btnA = !digitalRead(BTN_A);
  bool btnB = !digitalRead(BTN_B);

  // Profile switching (Hotkey + A/B)
  // Hotkey + A = next profile (more sensitive)
  // Hotkey + B = previous profile (less sensitive)
  bool profileChanged = false;

  if (hotkey) {
    if (btnA && !prevBtnA && currentProfile < NUM_PROFILES - 1) {
      currentProfile++;
      currentPPR = GAME_PPR[currentProfile];
      profileChanged = true;
    }
    if (btnB && !prevBtnB && currentProfile > 0) {
      currentProfile--;
      currentPPR = GAME_PPR[currentProfile];
      profileChanged = true;
    }
  }
  prevBtnA = btnA;
  prevBtnB = btnB;

  const uint8_t buttons[] = {BTN_B, BTN_A, BTN_SELECT, BTN_START, BTN_HOTKEY};
  const uint8_t mouseButtons[] = {MOUSE_LEFT, MOUSE_RIGHT, MOUSE_SIDE, MOUSE_EXTRA, MOUSE_MIDDLE};

  // Blink LEDs if profile changed
  if (profileChanged) {
    blinkProfile(currentProfile);
  }

  for (uint8_t i = 0; i < 5; i++) {
    bool pressed = !digitalRead(buttons[i]);
    if (pressed && !Mouse.isPressed(mouseButtons[i])) {
      Mouse.press(mouseButtons[i]);
    } else if (!pressed && Mouse.isPressed(mouseButtons[i])) {
      Mouse.release(mouseButtons[i]);
    }
  }

  delay(RESPONSE_DELAY);

  // Calculate mouse and scroll movement (optimized integer math)
  int16_t pos = spinnerPos;  // Atomic read
  uint8_t currentSensitivity = getSensitivity(currentPPR);
  int16_t mousePos = (pos * SENSITIVITY_MULTIPLIER) / (currentSensitivity * SENSITIVITY_DIVISOR);
  int16_t scrollPos = pos / SCROLL_SENSITIVITY;

  int8_t mouseDelta = clamp8(mousePos - prevMousePos);
  int8_t scrollDelta = clamp8(scrollPos - prevScrollPos);

  // Apply dead zone - ignore small movements
  #if DEAD_ZONE > 0
    if (mouseDelta > -DEAD_ZONE && mouseDelta < DEAD_ZONE) mouseDelta = 0;
    if (scrollDelta > -DEAD_ZONE && scrollDelta < DEAD_ZONE) scrollDelta = 0;
  #endif

  prevMousePos += mouseDelta;
  prevScrollPos += scrollDelta;

  // Send mouse movement (horizontal + scroll inverted)
  Mouse.move(-mouseDelta, 0, scrollDelta);
}
