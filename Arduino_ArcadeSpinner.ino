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
// Spinner pulses per revolution
#define SPINNER_PPR 600

// Spinner/Mouse sensitivity
// 1 is more sensitive
// 999 is less sensitive
#define DEFAULT_SENSITIVITY 1
#define SPINNER_SENSITIVITY 500
#define MOUSE_SENSITIVITY 5

/////////////////////////////////////////////////////////////////

// Pins used by encoder
#define pinA 3
#define pinB 2
// Pins used by buttons
#define Button0 5 // Left button B
#define Button1 4 // Right button A
#define Button2 14 // Start
#define Button3 15 // Hotkey
#define Button4 6 // Select

////////////////////////////////////////////////////////

#include <HID-Project.h>
#define MOUSE_SIDE      (1 << 3)
#define MOUSE_EXTRA     (1 << 4)

// Default virtual spinner position
int16_t drvpos = 0;
// Default real spinner position
int16_t r_drvpos = 0;
// Default virtual mouse position
int16_t m_drvpos = 0;

// Variables for paddle_emu
#define SP_MAX ((SPINNER_PPR*4*270UL)/360)
int32_t sp_clamp = SP_MAX/2;

// Response delay of the mouse, in ms
int responseDelay = 10;

// Interrupt pins of Rotary Encoder
void drv_proc()
{
  static int8_t prev = drvpos;
  int8_t a = digitalRead(pinA);
  int8_t b = digitalRead(pinB);

  int8_t spval = (b << 1) | (b^a);
  int8_t diff = (prev - spval)&3;

  if(diff == 1)
  {
    r_drvpos += 1;
    if(sp_clamp < SP_MAX) sp_clamp++;
  }
  if(diff == 3)
  {
    r_drvpos -= 1;
    if(sp_clamp > 0) sp_clamp--;
  }

  drvpos = r_drvpos / SPINNER_SENSITIVITY;
  m_drvpos = r_drvpos / MOUSE_SENSITIVITY;
  prev = spval;
}

// Run at startup
void setup()
{
  // Encoder
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  // Init encoder reading
  drv_proc();
  // Attach interrupt to each pin of the encoder
  attachInterrupt(digitalPinToInterrupt(pinA), drv_proc, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB), drv_proc, CHANGE);

  // Initialize Button Pins
  pinMode(Button0, INPUT_PULLUP);
  pinMode(Button1, INPUT_PULLUP);
  pinMode(Button2, INPUT_PULLUP);
  pinMode(Button3, INPUT_PULLUP);
  pinMode(Button4, INPUT_PULLUP);
}

// Main loop
void loop()
{

  struct ButtonMapper
  {
    int mButton;
    int mMouseButton;
  };

  static const ButtonMapper buttonsToMouseButtons[] =
  {
    { Button0, MOUSE_LEFT },
    { Button1, MOUSE_RIGHT },
    { Button2, MOUSE_MIDDLE },
    { Button3, MOUSE_EXTRA },
    { Button4, MOUSE_SIDE },
  };

  for(const ButtonMapper& mapper : buttonsToMouseButtons)
  {
    if (!digitalRead(mapper.mButton) == HIGH)
    {
      if (!Mouse.isPressed(mapper.mMouseButton)) Mouse.press(mapper.mMouseButton);
    }
    else if (Mouse.isPressed(mapper.mMouseButton)) Mouse.release(mapper.mMouseButton);
  }

  delay(responseDelay);

  static uint16_t m_prev = 0;
  int16_t val = ((int16_t)(m_drvpos - m_prev));
  if(val>127) val = 127; else if(val<-127) val = -127;
  m_prev += val;
  Mouse.move(val, 0);
}
