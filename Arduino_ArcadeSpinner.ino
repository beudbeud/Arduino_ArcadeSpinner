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
 * Profile 0: Omega Race   - 64 PPR (Sega standard)
 * Profile 1: Tempest      - 72 PPR (Atari vector standard)
 * Profile 2: Tron/DoT     - 128 PPR
 * Profile 3: Blasteroids  - 288 PPR
 * Profile 4: Arkanoid     - 486 PPR (geared, very sensitive)
 *
 * Sensitivity = ENCODER_COUNTS / GAME_PPR
 * Lower value = more sensitive (fewer encoder counts per game unit)
 */

#define NUM_PROFILES 5

// Pre-calculated sensitivity divisors: ENCODER_COUNTS / GAME_PPR
// These values divide the raw encoder count to match original game feel
const uint8_t MOUSE_SENSITIVITY[NUM_PROFILES] = {
  ENCODER_COUNTS / 64,
  ENCODER_COUNTS / 72,
  ENCODER_COUNTS / 128,
  ENCODER_COUNTS / 288,
  ENCODER_COUNTS / 486
};

// Default profile (4 = Arkanoid, 1 = Tempest recommended for most games)
#define DEFAULT_PROFILE 4

// Scroll wheel sensitivity (fixed, higher = less sensitive)
#define SCROLL_SENSITIVITY 400

// Response delay in ms
#define RESPONSE_DELAY 10

// LED feedback duration in ms
#define LED_BLINK_TIME 80

/////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////

#include <HID-Project.h>

// Mouse button definitions
#define MOUSE_SIDE  (1 << 3)
#define MOUSE_EXTRA (1 << 4)

// Spinner state
volatile int16_t spinnerPos = 0;

// Current profile
uint8_t currentProfile = DEFAULT_PROFILE;
uint8_t currentSensitivity = MOUSE_SENSITIVITY[DEFAULT_PROFILE];

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

// Encoder interrupt handler
void encoderISR() {
  static uint8_t prev = 0;
  uint8_t a = digitalRead(PIN_A);
  uint8_t b = digitalRead(PIN_B);

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
      currentSensitivity = MOUSE_SENSITIVITY[currentProfile];
      profileChanged = true;
    }
    if (btnB && !prevBtnB && currentProfile > 0) {
      currentProfile--;
      currentSensitivity = MOUSE_SENSITIVITY[currentProfile];
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

  // Calculate mouse and scroll movement
  int16_t pos = spinnerPos;  // Atomic read
  int16_t mousePos = pos / currentSensitivity;
  int16_t scrollPos = pos / SCROLL_SENSITIVITY;

  int8_t mouseDelta = clamp8(mousePos - prevMousePos);
  int8_t scrollDelta = clamp8(scrollPos - prevScrollPos);

  prevMousePos += mouseDelta;
  prevScrollPos += scrollDelta;

  // Send mouse movement (horizontal + scroll inverted)
  Mouse.move(-mouseDelta, 0, scrollDelta);
}
