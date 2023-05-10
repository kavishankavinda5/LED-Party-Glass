#include <Adafruit_NeoPixel.h>
#include <FastLED.h>
#include <EEPROM.h>
#define PIN 26
#define PIN_2 27
#define N_PIXELS 16
#define N_PIXELS_2 16
#define BG 0
#define COLOR_ORDER GRB  // Try mixing up the letters (RGB, GBR, BRG, etc) for a whole new world of color combinations
#define BRIGHTNESS 5     // 0-255, higher number is brighter.
#define LED_TYPE WS2812B
#define MIC_PIN 13          // Microphone is attached to this analog pin
#define DC_OFFSET 0         // DC offset in mic signal - if unusure, leave 0
#define NOISE 200           // Noise/hum/interference in mic signal
#define SAMPLES 60          // Length of buffer for dynamic level adjustment
#define TOP (N_PIXELS + 2)  // Allow dot to go slightly off scale
#define PEAK_FALL 20        // Rate of peak falling dot
#define N_PIXELS_HALF (N_PIXELS / 2)
#define GRAVITY -9.81  // Downward (negative) acceleration of gravity in m/s^2
#define h0 1           // Starting height, in meters, of the ball (strip length)
#define NUM_BALLS 3    // Number of bouncing balls you want (recommend < 7, but 20 is fun in its own way)
#define SPEED .20      // Amount to increment RGB color by each cycle

int brightnessPin = 2, potPin = 4;

//config for balls
float h[NUM_BALLS];                        // An array of heights
float vImpact0 = sqrt(-2 * GRAVITY * h0);  // Impact velocity of the ball when it hits the ground if "dropped" from the top of the strip
float vImpact[NUM_BALLS];                  // As time goes on the impact velocity will change, so make an array to store those values
float tCycle[NUM_BALLS];                   // The time since the last time the ball struck the ground
int pos[NUM_BALLS];                        // The integer position of the dot on the strip (LED index)
long tLast[NUM_BALLS];                     // The clock time of the last ground strike
float COR[NUM_BALLS];                      // Coefficient of Restitution (bounce damping)

float
  greenOffset = 30,
  blueOffset = 150;

byte
  peak = 0,      // Used for falling dot
  dotCount = 0,  // Frame counter for delaying dot-falling speed
  volCount = 0;  // Frame counter for storing past volume data
int
  vol[SAMPLES],   // Collection of prior volume samples
  lvl = 10,       // Current "dampened" audio level
  minLvlAvg = 0,  // For dynamic adjustment of graph low & high
  maxLvlAvg = 512;

int brightnessValue, prevBrightnessValue;
int sensorDeviationBrightness = 1;
int sensitivityValue = 128;    // 0 - 255, initial value (value read from the potentiometer if useSensorValues = true)
int maxSensitivity = 2 * 255;  // let the 'volume' go up to 200%!
int ledBrightness = 255;       // 0 - 255, initial value (value read from the potentiometer if useSensorValues = true)
int val;


Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(N_PIXELS_2, PIN_2, NEO_GRB + NEO_KHZ800);

// FOR SYLON ETC
uint8_t thisbeat = 23;
uint8_t thatbeat = 28;
uint8_t thisfade = 2;   // How quickly does it fade? Lower = slower fade rate.
uint8_t thissat = 255;  // The saturation, where 255 = brilliant colours.
uint8_t thisbri = 255;

//FOR JUGGLE
uint8_t numdots = 4;   // Number of dots in use.
uint8_t faderate = 2;  // How long should the trails be. Very low value = longer trails.
uint8_t hueinc = 16;   // Incremental change in hue between each dot.
uint8_t thishue = 0;   // Starting hue.
uint8_t curhue = 0;
uint8_t thisbright = 255;  // How bright should the LED/display be.
uint8_t basebeat = 5;
uint8_t max_bright = 255;

// Vu meter 4
const uint32_t Red = strip.Color(255, 0, 0);
const uint32_t Yellow = strip.Color(255, 255, 0);
const uint32_t Green = strip.Color(0, 255, 0);
const uint32_t Blue = strip.Color(0, 0, 255);
const uint32_t White = strip.Color(255, 255, 255);
const uint32_t Dark = strip.Color(0, 0, 0);
unsigned int sample;

CRGB leds[N_PIXELS];

int myhue = 0;


// constants used here to set pin numbers:
//const int buttonPin = 0;  // the number of the pushbutton pin
const int buttonPin = 33;  // the number of the pushbutton pin

// Variables will change:

int buttonPushCounter = 0;  // counter for the number of button presses
int buttonState = 0;        // current state of the button
int lastButtonState = 0;

//Ripple variables
int color;
int center = 0;
int step = -1;
int maxSteps = 8;
float fadeRate = 0.80;
int diff;

//background color
uint32_t currentBg = random(256);
uint32_t nextBg = currentBg;

void setup() {
  delay(2000);  // power-up safety delay
  FastLED.addLeds<WS2812B, PIN, COLOR_ORDER>(leds, N_PIXELS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, PIN_2, COLOR_ORDER>(leds, N_PIXELS_2).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  memset(vol, 0, sizeof(vol));
  LEDS.addLeds<LED_TYPE, PIN, COLOR_ORDER>(leds, N_PIXELS);
  LEDS.addLeds<LED_TYPE, PIN_2, COLOR_ORDER>(leds, N_PIXELS_2);
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'
  strip2.begin();
  strip2.show();  // Initialize all pixels to 'off'

  strip.setBrightness(15);
  strip2.setBrightness(15);


  //initialize the serial port
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);

  for (int i = 0; i < NUM_BALLS; i++) {  // Initialize variables
    tLast[i] = millis();
    h[i] = h0;
    pos[i] = 0;             // Balls start on the ground
    vImpact[i] = vImpact0;  // And "pop" up at vImpact0
    tCycle[i] = 0;
    COR[i] = 0.90 - float(i) / pow(NUM_BALLS, 2);
  }
}

void loop() {

  brightnessValue = analogRead(brightnessPin);
  brightnessValue = map(brightnessValue, 0, 1023, 0, 255);

  if (abs(brightnessValue - prevBrightnessValue) > sensorDeviationBrightness) {
    ledBrightness = brightnessValue;
    strip.setBrightness(15);
    strip2.setBrightness(15);
    prevBrightnessValue = brightnessValue;
  }

  //for mic
  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;
  // end mic
  //buttonPushCounter = EEPROM.read(0);
  // read the pushbutton input pin:
  buttonState = digitalRead(buttonPin);
  // compare the buttonState to its previous state
  if (buttonState != lastButtonState) {
    // if the state has changed, increment the counter
    if (buttonState == HIGH) {
      // if the current state is HIGH then the button
      // wend from off to on:
      buttonPushCounter++;
      Serial.println("on");
      Serial.print("number of button pushes:  ");
      Serial.println(buttonPushCounter);
      if (buttonPushCounter >= 20) {
        buttonPushCounter = 1;
      }
      EEPROM.write(0, buttonPushCounter);

    } else {
      // if the current state is LOW then the button
      // wend from on to off:
      Serial.println("off");
    }
  }
  // save the current state as the last state,
  //for next time through the loop

  lastButtonState = buttonState;

  switch (buttonPushCounter) {

    case 1:
      buttonPushCounter == 1;
      {
        Vu1();
        break;
      }

    case 2:
      buttonPushCounter == 2;
      {
        Vu4();
        break;
      }

    case 3:
      buttonPushCounter == 3;
      {
        Vu3();
        break;
      }

    case 4:
      buttonPushCounter == 4;
      {
        soundp1c1();
        break;
      }

    case 5:
      buttonPushCounter == 5;
      {
        soundp2c1();
        break;
      }

    case 6:
      buttonPushCounter == 6;
      {
        soundp1c2();
        break;
      }

    case 7:
      buttonPushCounter == 7;
      {
        soundp2c2();
        break;
      }

    case 8:
      buttonPushCounter == 8;
      {
        soundp1c3();
        break;
      }

    case 9:
      buttonPushCounter == 9;
      {
        soundp2c3();
        break;
      }

    case 10:
      buttonPushCounter == 10;
      {
        soundp1c4();
        break;
      }

    case 11:
      buttonPushCounter == 11;
      {
        soundp2c4();
        break;
      }

    case 12:
      buttonPushCounter == 12;
      {
        rainbow(5);
        break;
      }

    case 13:
      buttonPushCounter == 13;
      {
        ripple();
        break;
      }

    case 14:
      buttonPushCounter == 14;
      {
        ripple2();
        break;
      }

    case 15:
      buttonPushCounter == 15;
      {
        colorWipePattern();
        break;
      }

    case 16:
      buttonPushCounter == 16;
      {
        rainbowCycle(5);
        break;
      }

    case 17:
      buttonPushCounter == 17;
      {
        theaterChasePattern();
        break;
      }

    case 18:
      buttonPushCounter == 18;
      {
        theaterChaseRainbow(25);
        break;
      }

    case 19:
      buttonPushCounter == 19;
      {
        colorWipe(strip.Color(0, 0, 0), 5);  // A Black
        break;
      }
  }
}

//------------------------------------------------ Sound React Patterns -----------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

void Vu4() {
  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;


  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top
  greenOffset += SPEED;
  blueOffset += SPEED;
  if (greenOffset >= 255) greenOffset = 0;
  if (blueOffset >= 255) blueOffset = 0;

  // Color pixels based on rainbow gradient
  for (i = 0; i < N_PIXELS_HALF; i++) {
    if (i >= height) {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
    } else {
      uint32_t color = Wheel(map(i, 0, N_PIXELS_HALF - 1, (int)greenOffset, (int)blueOffset));
      strip.setPixelColor(N_PIXELS_HALF - i - 1, color);
      strip.setPixelColor(N_PIXELS_HALF + i, color);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, color);
      strip2.setPixelColor(N_PIXELS_HALF + i, color);
    }
  }

  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS_HALF - 1) {
    uint32_t color = Wheel(map(peak, 0, N_PIXELS_HALF - 1, 30, 150));
    strip.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip.setPixelColor(N_PIXELS_HALF + peak, color);
    strip2.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip2.setPixelColor(N_PIXELS_HALF + peak, color);
  }

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }


  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//---------------------------------------------------------------------------------------------------------------------

void Vu1() {
  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;


  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  greenOffset += SPEED;
  blueOffset += SPEED;
  if (greenOffset >= 255) greenOffset = 0;
  if (blueOffset >= 255) blueOffset = 0;

  // Color pixels based on rainbow gradient for strip 1
  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
    } else {
      strip.setPixelColor(i, Wheel(
                               map(i, 0, strip.numPixels() - 1, (int)greenOffset, (int)blueOffset)));
    }
  }

  // Color pixels based on rainbow gradient for strip 2
  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip2.setPixelColor(i, 0, 0, 0);
    } else {
      strip2.setPixelColor(i, Wheel(
                                map(i, 0, strip2.numPixels() - 1, (int)greenOffset, (int)blueOffset)));
    }
  }

  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  if (peak > 0 && peak <= N_PIXELS - 1) strip2.setPixelColor(peak, Wheel(map(peak, 0, strip2.numPixels() - 1, 30, 150)));

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }
  strip.show();   // Update strip
  strip2.show();  // Update strip

  vol[volCount] = n;
  if (++volCount >= SAMPLES) {
    volCount = 0;
  }

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) {
      minLvl = vol[i];
    } else if (vol[i] > maxLvl) {
      maxLvl = vol[i];
    }
  }

  if ((maxLvl - minLvl) < TOP) {
    maxLvl = minLvl + TOP;
  }
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//---------------------------------------------------------------------------------------------------------------------

void Vu3() {
  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;


  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  greenOffset += SPEED;
  blueOffset += SPEED;
  if (greenOffset >= 255) greenOffset = 0;
  if (blueOffset >= 255) blueOffset = 0;

  // Color pixels based on rainbow gradient for strip 1
  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
    } else {
      strip.setPixelColor(i, Wheel(
                               map(i, 0, strip.numPixels() - 1, (int)greenOffset, (int)blueOffset)));
    }
  }

  // Color pixels based on rainbow gradient for strip 2
  for (i = 0; i < N_PIXELS; i++) {
    int reverseIndex = (N_PIXELS - 1) - i;  // Calculate the reverse index
    if (reverseIndex >= height) {
      strip2.setPixelColor(i, 0, 0, 0);
    } else {
      strip2.setPixelColor(i, Wheel(
                                map(reverseIndex, 0, strip2.numPixels() - 1, (int)greenOffset, (int)blueOffset)));
    }
  }


  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  if (peak > 0 && peak <= N_PIXELS - 1) strip2.setPixelColor(peak, Wheel(map(peak, 0, strip2.numPixels() - 1, 30, 150)));

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }
  strip.show();   // Update strip
  strip2.show();  // Update strip

  vol[volCount] = n;
  if (++volCount >= SAMPLES) {
    volCount = 0;
  }

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) {
      minLvl = vol[i];
    } else if (vol[i] > maxLvl) {
      maxLvl = vol[i];
    }
  }

  if ((maxLvl - minLvl) < TOP) {
    maxLvl = minLvl + TOP;
  }
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//---------------------------------------------------------------------------------------------------------------------

void soundp1c1() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
      strip2.setPixelColor(i, 0, 0, 0);
    } else if (i < N_PIXELS / 4) {
      strip.setPixelColor(i, strip.Color(0, 245, 255));
      strip2.setPixelColor(i, strip2.Color(0, 245, 255));
    }  // set color to blue for first quarter of strip
    else if (i < N_PIXELS / 2) {
      strip.setPixelColor(i, strip.Color(252, 231, 0));
      strip2.setPixelColor(i, strip2.Color(252, 231, 0));
    }  // set color to red for second quarter of strip
    else if (i < N_PIXELS * 3 / 4) {
      strip.setPixelColor(i, strip.Color(255, 109, 40));
      strip2.setPixelColor(i, strip2.Color(255, 109, 40));
    }  // set color to yellow for third quarter of strip
    else {
      strip.setPixelColor(i, strip.Color(234, 4, 126));
      strip2.setPixelColor(i, strip2.Color(234, 4, 126));
    }  // set color to green for fourth quarter of strip
  }


  //--------------------------------------------------------------------------------------------------------------------
  // Draw peak dot
  // if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  //--------------------------------------------------------------------------------------------------------------------


  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp1c2() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
      strip2.setPixelColor(i, 0, 0, 0);
    } else if (i < N_PIXELS / 4) {
      strip.setPixelColor(i, strip.Color(73, 255, 0));
      strip2.setPixelColor(i, strip2.Color(73, 255, 0));
    }  // set color to blue for first quarter of strip
    else if (i < N_PIXELS / 2) {
      strip.setPixelColor(i, strip.Color(251, 255, 0));
      strip2.setPixelColor(i, strip2.Color(251, 255, 0));
    }  // set color to red for second quarter of strip
    else if (i < N_PIXELS * 3 / 4) {
      strip.setPixelColor(i, strip.Color(255, 147, 0));
      strip2.setPixelColor(i, strip2.Color(255, 147, 0));
    }  // set color to yellow for third quarter of strip
    else {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
      strip2.setPixelColor(i, strip2.Color(255, 0, 0));
    }  // set color to green for fourth quarter of strip
  }


  //--------------------------------------------------------------------------------------------------------------------
  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  if (peak > 0 && peak <= N_PIXELS - 1) strip2.setPixelColor(peak, Wheel(map(peak, 0, strip2.numPixels() - 1, 30, 150)));
  //--------------------------------------------------------------------------------------------------------------------


  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp1c3() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
      strip2.setPixelColor(i, 0, 0, 0);
    } else if (i < N_PIXELS / 4) {
      strip.setPixelColor(i, strip.Color(196, 0, 255));
      strip2.setPixelColor(i, strip2.Color(196, 0, 255));
    }  // set color to blue for first quarter of strip
    else if (i < N_PIXELS / 2) {
      strip.setPixelColor(i, strip.Color(255, 103, 231));
      strip2.setPixelColor(i, strip2.Color(255, 103, 231));
    }  // set color to red for second quarter of strip
    else if (i < N_PIXELS * 3 / 4) {
      strip.setPixelColor(i, strip.Color(255, 243, 56));
      strip2.setPixelColor(i, strip2.Color(255, 243, 56));
    }  // set color to yellow for third quarter of strip
    else {
      strip.setPixelColor(i, strip.Color(12, 236, 221));
      strip2.setPixelColor(i, strip2.Color(12, 236, 221));
    }  // set color to green for fourth quarter of strip
  }

  //--------------------------------------------------------------------------------------------------------------------
  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  if (peak > 0 && peak <= N_PIXELS - 1) strip2.setPixelColor(peak, Wheel(map(peak, 0, strip2.numPixels() - 1, 30, 150)));
  //--------------------------------------------------------------------------------------------------------------------


  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//---------------------------------------------------------------------------------------------------------------------

void soundp1c4() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);

  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }

  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  for (i = 0; i < N_PIXELS; i++) {
    if (i >= height) {
      strip.setPixelColor(i, 0, 0, 0);
      strip2.setPixelColor(i, 0, 0, 0);
    } else if (i < N_PIXELS / 4) {
      strip.setPixelColor(i, strip.Color(114, 255, 255));
      strip2.setPixelColor(i, strip2.Color(114, 255, 255));
    }  // set color to blue for first quarter of strip
    else if (i < N_PIXELS / 2) {
      strip.setPixelColor(i, strip.Color(0, 215, 255));
      strip2.setPixelColor(i, strip2.Color(0, 215, 255));
    }  // set color to red for second quarter of strip
    else if (i < N_PIXELS * 3 / 4) {
      strip.setPixelColor(i, strip.Color(0, 150, 255));
      strip2.setPixelColor(i, strip2.Color(0, 150, 255));
    }  // set color to yellow for third quarter of strip
    else {
      strip.setPixelColor(i, strip.Color(88, 0, 255));
      strip2.setPixelColor(i, strip2.Color(88, 0, 255));
    }  // set color to green for fourth quarter of strip
  }


  //--------------------------------------------------------------------------------------------------------------------
  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS - 1) strip.setPixelColor(peak, Wheel(map(peak, 0, strip.numPixels() - 1, 30, 150)));
  if (peak > 0 && peak <= N_PIXELS - 1) strip2.setPixelColor(peak, Wheel(map(peak, 0, strip2.numPixels() - 1, 30, 150)));
  //--------------------------------------------------------------------------------------------------------------------


  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp2c1() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);
  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }
  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  // Color pixels based on rainbow gradient
  for (i = 0; i < N_PIXELS_HALF; i++) {
    if (i >= height) {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
    } else {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 240, 12, 126);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 255, 255);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 240, 12, 126);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 255, 255);
    }
  }

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp2c2() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);
  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }
  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  // Color pixels based on rainbow gradient
  for (i = 0; i < N_PIXELS_HALF; i++) {
    if (i >= height) {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
    } else {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 255, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 73, 255, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 255, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 73, 255, 0);
    }
  }

  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS_HALF - 1) {
    uint32_t color = Wheel(map(peak, 0, N_PIXELS_HALF - 1, 30, 150));
    strip.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip.setPixelColor(N_PIXELS_HALF + peak, color);
    strip2.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip2.setPixelColor(N_PIXELS_HALF + peak, color);
  }

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp2c3() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);
  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }
  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  // Color pixels based on rainbow gradient
  for (i = 0; i < N_PIXELS_HALF; i++) {
    if (i >= height) {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
    } else {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 255, 240, 36);
      strip.setPixelColor(N_PIXELS_HALF + i, 255, 0, 77);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 255, 240, 36);
      strip2.setPixelColor(N_PIXELS_HALF + i, 255, 0, 77);
    }
  }

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//--------------------------------------------------------------------------------------------------------------------

void soundp2c4() {

  uint8_t i;
  uint16_t minLvl, maxLvl;
  int n, height;

  val = (analogRead(potPin));
  val = map(val, 0, 1023, -10, 6);
  n = analogRead(MIC_PIN);             // Raw reading from mic
  n = abs(n - 0 - DC_OFFSET);          // Center on zero
  n = (n <= NOISE) ? 0 : (n - NOISE);  // Remove noise/hum

  if (val < 0) {
    n = n / (val * (-1));
  }
  if (val > 0) {
    n = n * val;
  }
  lvl = ((lvl * 7) + n) >> 3;  // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if (height < 0L) height = 0;  // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak) peak = height;  // Keep 'peak' dot at top

  // Color pixels based on rainbow gradient
  for (i = 0; i < N_PIXELS_HALF; i++) {
    if (i >= height) {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 0, 0, 0);
      strip2.setPixelColor(N_PIXELS_HALF + i, 0, 0, 0);
    } else {
      strip.setPixelColor(N_PIXELS_HALF - i - 1, 12, 236, 221);
      strip.setPixelColor(N_PIXELS_HALF + i, 255, 243, 56);
      strip2.setPixelColor(N_PIXELS_HALF - i - 1, 12, 236, 221);
      strip2.setPixelColor(N_PIXELS_HALF + i, 255, 243, 56);
    }
  }

  // Draw peak dot
  if (peak > 0 && peak <= N_PIXELS_HALF - 1) {
    uint32_t color = Wheel(map(peak, 0, N_PIXELS_HALF - 1, 30, 150));
    strip.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip.setPixelColor(N_PIXELS_HALF + peak, color);
    strip2.setPixelColor(N_PIXELS_HALF - peak - 1, color);
    strip2.setPixelColor(N_PIXELS_HALF + peak, color);
  }

  strip.show();   // Update strip
  strip2.show();  // Update strip

  // Every few frames, make the peak pixel drop by 1:

  if (++dotCount >= PEAK_FALL) {  //fall rate

    if (peak > 0) peak--;
    dotCount = 0;
  }

  vol[volCount] = n;                        // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;  // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i = 1; i < SAMPLES; i++) {
    if (vol[i] < minLvl) minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }

  if ((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;  // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;  // (fake rolling average)
}

//---------------------------------------------- End Sound React Patterns ---------------------------------------------
//--------------------------------------------------------------------------------------------------------------------



//-------------------------------------------------- Normal patterns --------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------


// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

//---------------------------------------------------------------------------------------------------------------------

void ripple() {

  if (currentBg == nextBg) {
    nextBg = random(256);
  } else if (nextBg > currentBg) {
    currentBg++;
  } else {
    currentBg--;
  }
  for (uint16_t l = 0; l < N_PIXELS; l++) {
    //leds[l] = CHSV(currentBg, 255, 50);
    strip.setPixelColor(l, Wheel(currentBg, 0.1));
    strip2.setPixelColor(l, Wheel(currentBg, 0.1));
  }

  if (step == -1) {
    center = random(N_PIXELS);
    color = random(256);
    step = 0;
  }

  if (step == 0) {
    //leds[center] = CHSV(color, 255, 255);
    strip.setPixelColor(center, Wheel(color, 1));
    strip2.setPixelColor(center, Wheel(color, 1));
    step++;
  } else {
    if (step < maxSteps) {
      Serial.println(pow(fadeRate, step));

      // leds[wrap(center + step)] = CHSV(color, 255, pow(fadeRate, step) * 255);
      strip.setPixelColor(wrap(center + step), Wheel(color, pow(fadeRate, step)));
      strip2.setPixelColor(wrap(center + step), Wheel(color, pow(fadeRate, step)));
      //leds[wrap(center - step)] = CHSV(color, 255, pow(fadeRate, step) * 255);
      strip.setPixelColor(wrap(center - step), Wheel(color, pow(fadeRate, step)));
      strip2.setPixelColor(wrap(center - step), Wheel(color, pow(fadeRate, step)));
      if (step > 3) {
        //leds[wrap(center + step - 3)] = CHSV(color, 255, pow(fadeRate, step - 2) * 255);
        strip.setPixelColor(wrap(center + step - 3), Wheel(color, pow(fadeRate, step - 2)));
        strip2.setPixelColor(wrap(center + step - 3), Wheel(color, pow(fadeRate, step - 2)));
        // leds[wrap(center - step + 3)] = CHSV(color, 255, pow(fadeRate, step - 2) * 255);
        strip.setPixelColor(wrap(center - step + 3), Wheel(color, pow(fadeRate, step - 2)));
        strip2.setPixelColor(wrap(center - step + 3), Wheel(color, pow(fadeRate, step - 2)));
      }
      step++;
    } else {
      step = -1;
    }
  }

  //LEDS.show();
  strip.show();
  strip2.show();
  //delay(50);
  if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
    return;
  delay(50);
}

//---------------------------------------------------------------------------------------------------------------------

int wrap(int step) {
  if (step < 0) return N_PIXELS + step;
  if (step > N_PIXELS - 1) return step - N_PIXELS;
  return step;
}


void one_color_allHSV(int ahue, int abright) {  // SET ALL LEDS TO ONE COLOR (HSV)
  for (int i = 0; i < N_PIXELS; i++) {
    leds[i] = CHSV(ahue, 255, abright);
  }
}

//---------------------------------------------------------------------------------------------------------------------

void ripple2() {
  if (BG) {
    if (currentBg == nextBg) {
      nextBg = random(256);
    } else if (nextBg > currentBg) {
      currentBg++;
    } else {
      currentBg--;
    }
    for (uint16_t l = 0; l < N_PIXELS; l++) {
      strip.setPixelColor(l, Wheel(currentBg, 0.1));
      strip2.setPixelColor(l, Wheel(currentBg, 0.1));
    }
  } else {
    for (uint16_t l = 0; l < N_PIXELS; l++) {
      strip.setPixelColor(l, 0, 0, 0);
      strip2.setPixelColor(l, 0, 0, 0);
    }
  }


  if (step == -1) {
    center = random(N_PIXELS);
    color = random(256);
    step = 0;
  }



  if (step == 0) {
    strip.setPixelColor(center, Wheel(color, 1));
    strip2.setPixelColor(center, Wheel(color, 1));
    step++;
  } else {
    if (step < maxSteps) {
      strip.setPixelColor(wrap(center + step), Wheel(color, pow(fadeRate, step)));
      strip.setPixelColor(wrap(center - step), Wheel(color, pow(fadeRate, step)));
      strip2.setPixelColor(wrap(center + step), Wheel(color, pow(fadeRate, step)));
      strip2.setPixelColor(wrap(center - step), Wheel(color, pow(fadeRate, step)));
      if (step > 3) {
        strip.setPixelColor(wrap(center + step - 3), Wheel(color, pow(fadeRate, step - 2)));
        strip.setPixelColor(wrap(center - step + 3), Wheel(color, pow(fadeRate, step - 2)));
        strip2.setPixelColor(wrap(center + step - 3), Wheel(color, pow(fadeRate, step - 2)));
        strip2.setPixelColor(wrap(center - step + 3), Wheel(color, pow(fadeRate, step - 2)));
      }
      step++;
    } else {
      step = -1;
    }
  }

  strip.show();
  strip2.show();
  delay(50);
}

//---------------------------------------------------------------------------------------------------------------------

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos, float opacity) {

  if (WheelPos < 85) {
    return strip.Color((WheelPos * 3) * opacity, (255 - WheelPos * 3) * opacity, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color((255 - WheelPos * 3) * opacity, 0, (WheelPos * 3) * opacity);
  } else {
    WheelPos -= 170;
    return strip.Color(0, (WheelPos * 3) * opacity, (255 - WheelPos * 3) * opacity);
  }
}

//---------------------------------------------------------------------------------------------------------------------

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip2.setPixelColor(i, c);
    strip.show();
    strip2.show();
    if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
      return;                                       // <------------ and this
    delay(wait);
  }
}

//---------------------------------------------------------------------------------------------------------------------

void colorWipeNew(uint32_t c, uint8_t wait) {

  int i = 0;
  int j = strip2.numPixels() - 1;

  while (i < strip.numPixels() && j >= 0) {
    strip.setPixelColor(i, c);
    strip2.setPixelColor(j, c);
    strip.show();
    strip2.show();
    i++;
    j--;
    if (digitalRead(buttonPin) != lastButtonState)
      return;
    delay(wait);
  }

  i = strip.numPixels() - 1;
  j = 0;

  while (i >= 0 && j < strip2.numPixels()) {
    strip2.setPixelColor(j, c);
    strip.setPixelColor(i, c);
    strip.show();
    strip2.show();
    i--;
    j++;
    if (digitalRead(buttonPin) != lastButtonState)
      return;
    delay(wait);
  }
}

//--------------------------------------------------------------------------------------------------------------------

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j = 0; j < 10; j++) {  //do 10 cycles of chasing
    for (int q = 0; q < 3; q++) {
      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, c);   //turn every third pixel on
        strip2.setPixelColor(i + q, c);  //turn every third pixel on
      }
      strip.show();
      strip2.show();
      if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
        return;                                       // <------------ and this
      delay(wait);

      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, 0);   //turn every third pixel off
        strip2.setPixelColor(i + q, 0);  //turn every third pixel off
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
      strip2.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    strip2.show();
    if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
      return;
    delay(wait);
  }
}

//--------------------------------------------------------------------------------------------------------------------

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) {  // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
      strip2.setPixelColor(i, Wheel(((i * 256 / strip2.numPixels()) + j) & 255));
    }
    strip.show();
    strip2.show();
    if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
      return;                                       // <------------ and this
    delay(wait);
  }
}

//--------------------------------------------------------------------------------------------------------------------

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j = 0; j < 256; j++) {  // cycle all 256 colors in the wheel
    for (int q = 0; q < 3; q++) {
      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, Wheel((i + j) % 255));   //turn every third pixel on
        strip2.setPixelColor(i + q, Wheel((i + j) % 255));  //turn every third pixel on
      }
      strip.show();
      strip2.show();
      if (digitalRead(buttonPin) != lastButtonState)  // <------------- add this
        return;                                       // <------------ and this
      delay(wait);

      for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, 0);   //turn every third pixel off
        strip2.setPixelColor(i + q, 0);  //turn every third pixel off
      }
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------- colorWipePattern -------------------------------------------------------

void colorWipePattern() {

  colorWipeNew(strip.Color(255, 0, 0), 40);
  colorWipeNew(strip.Color(0, 255, 0), 40);
  colorWipeNew(strip.Color(0, 0, 255), 40);
}

//------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------- theaterChasePattern -------------------------------------------------------

void theaterChasePattern() {

  // Send a theater pixel chase in...
  theaterChase(strip.Color(0, 255, 0), 50);
  theaterChase(strip.Color(0, 255, 255), 50);
  theaterChase(strip.Color(255, 255, 0), 50);
}

//-------------------------------------------------------------------------------------------------------------------------------
