//--------------------------------------------------------------------
//                       Includes
//--------------------------------------------------------------------
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"
#include "bmp.h"               // Image file
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>

/**
 * @file    Solar Charge Screen
 * @brief   Firmware for the WALL-E LCD Screen & Battery Monitor.
 * @version 1.1
 *
 * @details This firmware monitors battery voltage, displays it on an LCD screen,
 *          provides audio feedback, and allows for calibration. It uses an
 *          RA8875 display, DFPlayer Mini for audio, NeoPixels for visual alerts,
 *          and EEPROM for persistent storage.
 *
 * @author  John Geb]
 * @date    [2-25-25]
 *
 * Revision History:
 *
 * Version 1.1 - [Current Version]
 *  - Feature: Implemented EEPROM integration for persistent storage of calibration settings, ensuring settings are retained across power cycles.
 *  - Enhancement: Added RA8875_ORANGE color definition for improved UI aesthetics within calibration menus.
 *  - Enhancement: Introduced 'isAudio4Playing' flag to manage low-battery audio looping, preventing audio interruption and improving user experience.
 *  - Core: Defined EEPROM address constants (OFFSET_ADDRESS, VOLTAGE_THRESHOLDS_ADDRESS) for organized memory management.
 *  - Core: Integrated voltageThresholds array (NUM_VOLTAGE_THRESHOLDS) for dynamic battery level thresholds, enabling adjustable battery indicators.
 *  - Feature: Implemented comprehensive calibration mode control (isCalibrating, inSubMenu, timing variables) for precise battery monitoring.
 *  - Function: Integrated checkCalibrationEntry() function to monitor Stop button for long-press gesture to activate calibration.
 *  - Function: Created calibrationMenu() for user-friendly interactive calibration (offset and thresholds).
 *  - Function: Developed updateCalibrationDisplay() and updateVoltageDisplay() functions for calibration menu UI and data presentation.
 *  - Core: Enhanced setup() routine with EEPROM load/initialization logic, including data validation and default value handling.
 *  - Behavior: Implemented low-battery warning system (blinking red indicator, audio loop) based on voltageThresholds[0].
 *  - I/O: Moved NeoPixel trigger logic ahead of button handling in loop() for improved responsiveness.
 *  - I/O: Configured Light button in loop() to trigger screen clear rather than reset.
 *  - UI: Streamlined boot sequence timing.
 *
 * Version 1.0 - [Thank you Dan Coe For Orignal Code]   
 *  - Functionality for battery voltage monitoring and LCD display.
 *  - Button control for sound playback.
 */

//--------------------------------------------------------------------
//                       Definitions
//--------------------------------------------------------------------

// RA8875 Display Pins
#define RA8875_CS 10
#define RA8875_RESET 9
#define RA8875_ORANGE 0xFD20  // 16-bit RGB (R: 31, G: 101, B: 0)

// EEPROM Addresses
#define OFFSET_ADDRESS 0
#define VOLTAGE_THRESHOLDS_ADDRESS 4

// Pin Assignments for Buttons
#define PLAY_BUTTON 6
#define STOP_BUTTON 7
#define RECORD_BUTTON 8
#define LIGHT_BUTTON 5

// Pin Assignments for NeoPixel
#define NEOPIXEL_PIN 4
#define TRIGGER_PIN 3
#define NUM_PIXELS 7

// Pin Assignments for DFPlayer
#define DFPLAYER_RX 2  // Connect to TX of DFPlayer
#define DFPLAYER_TX 15 // Connect to RX of DFPlayer

#define BUFFER_VOLTAGE 0.1 //Buffer For Yellow Box Flash
//--------------------------------------------------------------------
//                       Global Variables
//--------------------------------------------------------------------

// Flags
bool isAudio4Playing = false;

// RA8875 Display Instance
Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);

// Voltage Thresholds for LifePO4 Battery
const int NUM_VOLTAGE_THRESHOLDS = 10;
float voltageThresholds[NUM_VOLTAGE_THRESHOLDS] = {
    12.48,  //0%
    12.96,  //11.1%
    13.44,  //22.2%
    13.92,  //33.3%
    14.40,  //44.4%
    14.88,  //55.6%
    15.36,  //66.7%
    15.84,  //77.8%
    16.32,  //88.9%
    16.80   //100%
};

byte currentThresholdIndex = 0;
bool voltageCalMode = false;

// Timing Constants
const unsigned long POWER_CHECK_DELAY = 1000;   // Delay for power level check
const unsigned long DEBOUNCE_DELAY = 1000;      // Debounce delay in milliseconds
const unsigned long DOUBLE_PRESS_TIME = 750;     // Max time between double press (milliseconds)
const unsigned long HOLD_TIME = 1500;           // Hold time to enter calibration mode (milliseconds)

// Voltage Sensor
int voltageSensor1 = A0;

// Voltage Calculation Variables
float vOUT1 = 0.0;
float vIN1 = 0.0;
float R11 = 39000;
float R22 = 9100;
int value1 = 0;

// Calibration Offset Value
float offset = 1.38;

// UI Constants
byte round_corner = 4;
byte start_x = 90;

// Button Debounce Variables
unsigned long lastPressTimeLightbutton = 0;
unsigned long lastPressTimePlaybutton = 0;
unsigned long lastPressTimeStopbutton = 0;
unsigned long lastPressTimeRecordbutton = 0;

unsigned long firstPressTime = 0;
unsigned long holdStartTime = 0;
bool stopButtonPressedOnce = false;
bool holdingStopButton = false;
bool isCalibrating = false;
bool inSubMenu = false;

// Audio Duration and Delay
unsigned long lastAudioStartTime = 0;
const unsigned long AUDIO_DURATION = 250;  
const unsigned long AUDIO_DELAY = 0;     

//--------------------------------------------------------------------
//                       Objects
//--------------------------------------------------------------------

// DFPlayer Mini Instance
DFRobotDFPlayerMini myDFPlayer;

// NeoPixel Strip Instance
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Software Serial Instance for DFPlayer
SoftwareSerial mySoftwareSerial(DFPLAYER_RX, DFPLAYER_TX); // RX, TX

//--------------------------------------------------------------------
//                       Function Declarations
//--------------------------------------------------------------------

void checkCalibrationEntry();
void calibrationMenu();
void updateCalibrationDisplay();
void updateVoltageDisplay();
void boot_sequence();

//--------------------------------------------------------------------
//                       Functions
//--------------------------------------------------------------------

/**
 * @brief Checks for the long-press gesture on the Stop button to activate calibration mode.
 */
void checkCalibrationEntry() {
    unsigned long now = millis();

    if (digitalRead(STOP_BUTTON) == LOW) { // Button is being held
        if (!stopButtonPressedOnce) {
            stopButtonPressedOnce = true;
            firstPressTime = now;
        }
        else if (!holdingStopButton && (now - firstPressTime <= DOUBLE_PRESS_TIME)) {
            holdingStopButton = true;
            holdStartTime = now;
        }

        // If holding the button long enough, immediately toggle calibration mode
        if (holdingStopButton && (now - holdStartTime >= HOLD_TIME)) {
            isCalibrating = !isCalibrating;
            tft.fillScreen(isCalibrating ? RA8875_BLACK : RA8875_BLACK);
            Serial.println(isCalibrating ? "Entering Calibration Mode" : "Exiting Calibration Mode");
            delay(1000);
            holdingStopButton = false; // Reset to prevent multiple toggles
        }
    }
    else {  // Button released
        stopButtonPressedOnce = false;
        holdingStopButton = false;
    }
}

/**
 * @brief Displays and handles the calibration menu on the LCD screen.
 */
void calibrationMenu() {
    tft.fillScreen(RA8875_BLACK);
    tft.setRotation(3);
    tft.writeReg(0x22, 0x10);
    tft.writeReg(0x20, 0x07);
    tft.textMode();
    tft.textColor(RA8875_ORANGE, RA8875_BLACK);
    tft.textEnlarge(1);

    if (!inSubMenu) {
        // Main selection menu
        tft.textSetCursor(120, 30);
        tft.textWrite("Select Calibration Mode:");

        tft.textSetCursor(200, 30);
        tft.textWrite("Play: Offset Calibration");

        tft.textSetCursor(280, 30);
        tft.textWrite("Stop: Voltage Thresholds");

        tft.textSetCursor(360, 30);
        tft.textWrite("Light: Exit"); // Changed from Record to Light

        while (isCalibrating && !inSubMenu) {
            if (digitalRead(PLAY_BUTTON) == LOW) {
                voltageCalMode = false;
                inSubMenu = true;
                delay(500);
                calibrationMenu();
            }
            if (digitalRead(STOP_BUTTON) == LOW) {
                voltageCalMode = true;
                inSubMenu = true;
                currentThresholdIndex = 0;
                delay(500);
                calibrationMenu();
            }
            if (digitalRead(LIGHT_BUTTON) == LOW) { // Changed from Recordbutton to Lightbutton
                isCalibrating = false;
                delay(500);
                tft.fillScreen(RA8875_BLACK);
            }
        }
    } else {
        // Sub-menus (existing calibration menus)
        if (!voltageCalMode) {
            // Offset calibration menu
            tft.textSetCursor(120, 30);
            tft.textWrite("Offset Calibration Mode");

            tft.textSetCursor(200, 30);
            tft.textWrite("Play: Increase Offset");

            tft.textSetCursor(280, 30);
            tft.textWrite("Stop: Decrease Offset");

            tft.textSetCursor(500, 30);
            tft.textWrite("Record: Save");

            tft.textSetCursor(650, 30);
            tft.textWrite("Light: Back to Menu");

            updateCalibrationDisplay();
        } else {
            // Voltage threshold calibration menu
            tft.textSetCursor(120, 10);
            tft.textWrite("Voltage Threshold Calibration");

            tft.textSetCursor(200, 50);
            tft.textWrite("Play: Inc Threshold");

            tft.textSetCursor(280, 50);
            tft.textWrite("Stop: Dec Threshold");

            tft.textSetCursor(360, 50);
            tft.textWrite("Record: Next Level");

            tft.textSetCursor(680, 50);
            tft.textWrite("Light: Back to Menu");

            updateVoltageDisplay();
        }

        // Handle sub-menu controls
        while (isCalibrating && inSubMenu) {
            if (!voltageCalMode) {
                // Offset calibration controls
                if (digitalRead(PLAY_BUTTON) == LOW) {
                    offset += 0.01;
                    if (isnan(offset)) offset = 1.38;
                    updateCalibrationDisplay();
                    delay(300);
                }
                if (digitalRead(STOP_BUTTON) == LOW) {
                    offset -= 0.01;
                    if (isnan(offset)) offset = 1.38;
                    updateCalibrationDisplay();
                    delay(300);
                }
                if (digitalRead(RECORD_BUTTON) == LOW) {
                    EEPROM.put(OFFSET_ADDRESS, offset);
                    inSubMenu = false;
                    delay(500);
                    calibrationMenu();
                }
            } else {
                // Voltage threshold calibration controls
                if (digitalRead(PLAY_BUTTON) == LOW) {
                    voltageThresholds[currentThresholdIndex] += 0.1;
                    updateVoltageDisplay();
                    delay(300);
                }
                if (digitalRead(STOP_BUTTON) == LOW) {
                    voltageThresholds[currentThresholdIndex] -= 0.1;
                    updateVoltageDisplay();
                    delay(300);
                }
                if (digitalRead(RECORD_BUTTON) == LOW) {
                    // Save to EEPROM before incrementing
                    EEPROM.put(VOLTAGE_THRESHOLDS_ADDRESS + (currentThresholdIndex * sizeof(float)), voltageThresholds[currentThresholdIndex]);

                    currentThresholdIndex++;
                    if (currentThresholdIndex >= NUM_VOLTAGE_THRESHOLDS) {
                        currentThresholdIndex = 0;
                    }
                    updateVoltageDisplay();
                    delay(500);
                }
            }

            // Common back button for both sub-menus
            if (digitalRead(LIGHT_BUTTON) == LOW) {
                inSubMenu = false;
                delay(500);
                calibrationMenu();
            }
        }
    }
}

/**
 * @brief Updates the calibration display with current offset and voltage readings.
 */
void updateCalibrationDisplay() {
    // Clear the previous values area
    tft.fillRect(150, 360, 400, 100, RA8875_BLACK);

    tft.setRotation(3);
    tft.textMode();
    tft.writeReg(0x22, 0x10);
    tft.writeReg(0x20, 0x07);
    tft.textColor(RA8875_ORANGE, RA8875_BLACK);
    tft.textEnlarge(1);

    // Update voltage reading
    value1 = analogRead(voltageSensor1);
    vOUT1 = (value1 * 5.0) / 1024.0;
    vIN1 = vOUT1 / (R22 / (R11 + R22));
    vIN1 -= offset;

    // Display current offset
    tft.textSetCursor(340, 30);
    char offsetBuffer[20];
    dtostrf(offset, 6, 2, offsetBuffer);
    tft.textWrite("Offset: ");
    tft.textWrite(offsetBuffer);

    // Display current voltage
    tft.textSetCursor(420, 30);
    char voltageBuffer[20];
    dtostrf(vIN1, 6, 2, voltageBuffer);
    tft.textWrite("Voltage: ");
    tft.textWrite(voltageBuffer);
    tft.textWrite("V");

    Serial.println("Offset: ");
    Serial.println(offset);
    Serial.println(" VIN1: ");
    Serial.println(vIN1);
}

/**
 * @brief Updates the voltage threshold display with the current threshold level.
 */
void updateVoltageDisplay() {
    tft.fillRect(150, 360, 400, 60, RA8875_BLACK);
    tft.setRotation(3);
    tft.textMode();
    tft.writeReg(0x22, 0x10);
    tft.writeReg(0x20, 0x07);
    tft.textColor(RA8875_ORANGE, RA8875_BLACK);
    tft.textEnlarge(1);

    char voltageStr[10]; // Buffer for voltage value
    dtostrf(voltageThresholds[currentThresholdIndex], 4, 2, voltageStr); // Convert float to string

    tft.textSetCursor(500, 50);
    String displayString = "Level " + String(currentThresholdIndex + 1) + ": " + voltageStr + "V";
    tft.textWrite(displayString.c_str());

    Serial.println("Adjusting Threshold ");
    Serial.println(currentThresholdIndex + 1);
    Serial.println(": ");
    Serial.println(voltageThresholds[currentThresholdIndex]);
}

//--------------------------------------------------------------------
//                       SETUP
//--------------------------------------------------------------------

/**
 * @brief Initializes the hardware and sets up the program.
 */
void setup() {
  Serial.begin(9600);   // Debug Console
  mySoftwareSerial.begin(9600); // Initialize software serial for DFPlayer

  // Initialize DFPlayer Mini using software serial instead of hardware serial
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println("DFPlayer Mini not found.");
    while (true); // Stay here if DFPlayer Mini is not found
  }
  Serial.println("DFPlayer Mini ready.");

  // Set the volume (0 to 30)
  myDFPlayer.volume(30);

  // initialize the Electroncis Panel pushbutton pins as an input:
  pinMode(PLAY_BUTTON, INPUT_PULLUP);
  pinMode(STOP_BUTTON, INPUT_PULLUP);
  pinMode(RECORD_BUTTON, INPUT_PULLUP);
  pinMode(LIGHT_BUTTON, INPUT_PULLUP);

  EEPROM.get(OFFSET_ADDRESS, offset); // Load saved offset
    if (isnan(offset)) {
    offset = 0.0;  // Reset to default
    EEPROM.put(OFFSET_ADDRESS, offset);  // Save default back to EEPROM
    Serial.println("Offset was NaN! Resetting to default 0.0");

      }

    // Load voltage thresholds from EEPROM
    bool eepromUninitialized = false;
    for (int i = 0; i < NUM_VOLTAGE_THRESHOLDS; i++) {
        EEPROM.get(VOLTAGE_THRESHOLDS_ADDRESS + (i * sizeof(float)), voltageThresholds[i]);

        // Check if the stored value is NaN or out of realistic range
        if (isnan(voltageThresholds[i]) || voltageThresholds[i] < 10.0 || voltageThresholds[i] > 20.0) {
            eepromUninitialized = true;
            voltageThresholds[i] = 12.48 + (i * 0.48); // Set default values dynamically
        }
    }

    // If EEPROM had uninitialized values, write default thresholds back
    if (eepromUninitialized) {
        for (int i = 0; i < NUM_VOLTAGE_THRESHOLDS; i++) {
            EEPROM.put(VOLTAGE_THRESHOLDS_ADDRESS + (i * sizeof(float)), voltageThresholds[i]);
        }
        Serial.println("EEPROM was uninitialized! Default voltage thresholds set.");
    }
// Set the trigger pin for NeoPixel as input
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  // Initialize NeoPixel strip
  strip.begin();
  strip.setBrightness(255); // Max brightness
  strip.show(); // Initialize the pixels to off

  pinMode(A0, INPUT);

    /* Initialize the display using 'RA8875_480x80', 'RA8875_480x128', 'RA8875_480x272' or 'RA8875_800x480' */
  if (!tft.begin(RA8875_800x480)) {
    Serial.println("RA8875 Not Found!");
    while (1);

  }
delay(250);
   myDFPlayer.play(1); // Play sound 1
    Serial.println("Playing sound 1.");
 
  tft.displayOn(true);
  tft.GPIOX(true);                              // Enable TFT - display enable tied to GPIOX
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024); // PWM output for backlight
  tft.PWM1out(255);                             // Set Display Brightness to maximum
  tft.setRotation(3);                           // 0,1,2 or 3  Changes screen rotation
  tft.fillScreen(RA8875_BLACK);                 // Clear the Screen



  tft.drawBitmap(start_x + 0, 0, sclbmp, 32, 480,RA8875_YELLOW);
  tft.drawBitmap(start_x + 77, 337, sun, 144, 144,RA8875_YELLOW);

  tft.fillRoundRect(start_x + 483, 0, 71, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 438, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 393, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 348, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 303, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 258, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 213, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 168, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 123, 0, 20, 294, round_corner, RA8875_BLACK);
  tft.fillRoundRect(start_x + 78, 0, 20, 294, round_corner, RA8875_BLACK);
  boot_sequence();
  
}

//********************************** END SETUP ***********************************//

//--------------------------------------------------------------------
//                       LOOP START
//--------------------------------------------------------------------

/**
 * @brief Main loop of the program.
 */
void loop() {

  //--------------------------------------------------------------------
  //                       Battery Monitoring
  //--------------------------------------------------------------------
    unsigned long currentMillis = millis();  // Update the current time
    unsigned long powerMillis = 0;
    // Check and update power levels
    if (currentMillis - powerMillis >= POWER_CHECK_DELAY) {  
        value1 = analogRead(voltageSensor1);
        vOUT1 = (value1 * 5.0) / 1024.0;
        vIN1 = vOUT1 / (R22 / (R11 + R22));
        vIN1 -= offset;  
        powerMillis = currentMillis;
    }

    // Check Battery Critical Level and Set Alert
   if (vIN1 >= voltageThresholds[0] + BUFFER_VOLTAGE) {
        // Battery voltage is above threshold -> Yellow box (No blinking)
        tft.fillRoundRect(start_x + 483, 0, 71, 294, round_corner, RA8875_YELLOW);
        
        // Stop audio 4 if it was playing
        if (isAudio4Playing) {
            myDFPlayer.stop();  
            isAudio4Playing = false;
            lastAudioStartTime = 0;
        }
    } else if (vIN1 <= voltageThresholds[0] - BUFFER_VOLTAGE) {
        // Below threshold -> Red blinking box & play audio 4
        unsigned long currentTime = millis();
        
        // Handle visual blinking
        if ((currentTime / 250) % 2 == 0) {
            tft.fillRoundRect(start_x + 483, 0, 71, 294, round_corner, RA8875_RED);
            
            // Handle audio playback
            if (!isAudio4Playing && (currentTime - lastAudioStartTime >= AUDIO_DURATION + AUDIO_DELAY)) {
                myDFPlayer.stop();  // Stop any previous playback
                delay(50);  // Short delay for DFPlayer to process
                myDFPlayer.play(4);
                isAudio4Playing = true;
                lastAudioStartTime = currentTime;
                Serial.println("Playing Audio 4");
            }
        } else {
            tft.fillRoundRect(start_x + 483, 0, 71, 294, round_corner, RA8875_BLACK);
        }
        
        // Check if audio should be marked as finished
        if (isAudio4Playing && (currentTime - lastAudioStartTime >= AUDIO_DURATION)) {
            isAudio4Playing = false;
        }
    }

// Update Battery Level Indicators based on Thresholds
for (int i = 1; i < NUM_VOLTAGE_THRESHOLDS; i++) {
    if (vIN1 >= voltageThresholds[i] + BUFFER_VOLTAGE) {
        tft.fillRoundRect(start_x + (483 - (i * 45)), 0, 20, 294, round_corner, RA8875_YELLOW);
    } else if (vIN1 <= voltageThresholds[i] - BUFFER_VOLTAGE) {
        tft.fillRoundRect(start_x + (483 - (i * 45)), 0, 20, 294, round_corner, RA8875_BLACK);
    }
}

 Serial.println(vIN1);

    // Calibration checks and call
    currentMillis = millis();
    checkCalibrationEntry();

   // Check if in calibration mode and call the calibration menu
    if (isCalibrating) {
        calibrationMenu();
    }

    //--------------------------------------------------------------------
    //                    NeoPixel Control
    //--------------------------------------------------------------------

    // Check if the TRIGGER_PIN is low (active low)
    int triggerState = digitalRead(TRIGGER_PIN);
    if (triggerState == LOW) {
        // If the trigger pin is low, turn the NeoPixel jewel red
        for (int i = 0; i < NUM_PIXELS; i++) {
            strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red color
        }
        strip.show();
    } else {
        // If the trigger pin is high, turn off the NeoPixel jewel
        for (int i = 0; i < NUM_PIXELS; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off color
        }
        strip.show();
    }
    delay(10);

    //--------------------------------------------------------------------
    //                    Electronic Panel Buttons
    //--------------------------------------------------------------------

    // Check for button presses and debounce

    if (digitalRead(RECORD_BUTTON) == LOW && (currentMillis - lastPressTimeRecordbutton) >= DEBOUNCE_DELAY) {
        myDFPlayer.play(3); // Play sound 3
        Serial.println("Playing sound 3.");
        lastPressTimeRecordbutton = currentMillis;
        delay(500);
    }

    if (digitalRead(PLAY_BUTTON) == LOW && (currentMillis - lastPressTimePlaybutton) >= DEBOUNCE_DELAY) {
        myDFPlayer.play(2); // Play sound 2
        Serial.println("Playing sound 2.");
        lastPressTimePlaybutton = currentMillis;
        delay(500);
    }

    if (digitalRead(STOP_BUTTON) == LOW && (currentMillis - lastPressTimeStopbutton) >= DEBOUNCE_DELAY) {
        myDFPlayer.stop(); // Stop any sound playing
        Serial.println("Sound stopped.");
        lastPressTimeStopbutton = currentMillis;
        delay(500);
    }

     if (digitalRead(LIGHT_BUTTON) == LOW && (currentMillis - lastPressTimeLightbutton) >= DEBOUNCE_DELAY) {
        Serial.println("Resetting Arduino");
        wdt_enable(WDTO_15MS);  // Enable the watchdog timer with a short timeout (15ms)
        while (true) {}  // The program will hang here until the reset occurs
        lastPressTimeLightbutton = currentMillis;
    }


}   //=================End Loop========================

/**
 * @brief  Initiates a boot sequence by illuminating the battery level indicators.
 */
void boot_sequence() {
    // Start Boot Sequence by turning the display ON
    tft.displayOn(true);
    tft.fillRoundRect(start_x + 483, 0, 71, 294, round_corner, RA8875_YELLOW);  // Index 0
    delay(500);
    tft.fillRoundRect(start_x + 438, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 1
    delay(700);
    tft.fillRoundRect(start_x + 393, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 2
    delay(550);
    tft.fillRoundRect(start_x + 348, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 3
    delay(500);
    tft.fillRoundRect(start_x + 303, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 4
    delay(450);
    tft.fillRoundRect(start_x + 258, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 5
    delay(400);
    tft.fillRoundRect(start_x + 213, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 6
    delay(350);
    tft.fillRoundRect(start_x + 168, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 7
    delay(200);
    tft.fillRoundRect(start_x + 123, 0, 20, 294, round_corner, RA8875_YELLOW);  // Index 8
    delay(150);
    tft.fillRoundRect(start_x + 78, 0, 20, 294, round_corner, RA8875_YELLOW);   // Index 9
}
