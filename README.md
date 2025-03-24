# Wall-E-Solar-Panel-Display
Solar Panel Display for 1-1 Scale Wall-E

[Thank you Dan Coe For Orignal Code] 

@file    Solar Charge Screen
 * @brief   Firmware for the WALL-E LCD Screen & Battery Monitor.
 * @version 2.1
 * * @details This firmware monitors battery voltage, displays it on an LCD screen,
 *          provides audio feedback, and allows for calibration. It uses an
 *          RA8875 display, DFPlayer Mini for audio, NeoPixels for visual alerts,
 *          and EEPROM for persistent storage.
 *
 * @author  John Geb]
 * @date    [2-25-25]
 *
 * Revision History:
 *
 * Version 2.1 - [Current Version]
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
 * Version 1.0 - [Orignal Code by Dan Coe]   
 *  - Functionality for battery voltage monitoring and LCD display.
 *  - Button control for sound playback.
