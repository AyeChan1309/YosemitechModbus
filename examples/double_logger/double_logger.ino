/*****************************************************************************
double_logger.ino
Written By:  Sara Damiano (sdamiano@stroudcenter.org)
Development Environment: PlatformIO 3.2.1
Hardware Platform: EnviroDIY Mayfly Arduino Datalogger
Software License: BSD-3.
  Copyright (c) 2017, Stroud Water Research Center (SWRC)
  and the EnviroDIY Development Team

This sketch is an example of logging data from different variables at two
different logging intervals.  This example uses more of the manual functions
in the logging loop rather than the simple "log" function.

DISCLAIMER:
THIS CODE IS PROVIDED "AS IS" - NO WARRANTY IS GIVEN.
*****************************************************************************/

// Select your modem chip, comment out all of the others
// #define TINY_GSM_MODEM_SIM800  // Select for anything using a SIM800, SIM900, or variant thereof: Sodaq GPRSBees, Microduino GPRS chips, Adafruit Fona, etc
// #define TINY_GSM_MODEM_A6  // Select for A6 or A7 chips
// #define TINY_GSM_MODEM_M590
// #define TINY_GSM_MODEM_ESP8266  // Select for an ESP8266 using the DEFAULT AT COMMAND FIRMWARE
#define TINY_GSM_MODEM_XBEE  // Select for Digi brand XBee's, including WiFi or LTE-M1

// ---------------------------------------------------------------------------
// Include the base required libraries
// ---------------------------------------------------------------------------
#define MODULAR_SENSORS_OUTPUT Serial  // Without this there will be no output
#include <Arduino.h>  // The base Arduino library
#include <EnableInterrupt.h>  // for external and pin change interrupts
#include <LoggerBase.h>
#include <ModemSupport.h>

// ---------------------------------------------------------------------------
// Set up the sensor specific information
//   ie, pin locations, addresses, calibrations and related settings
// ---------------------------------------------------------------------------
// The name of this file
const char *SKETCH_NAME = "logger_test.ino";

// Logger ID, also becomes the prefix for the name of the data file on SD card
const char *LoggerID = "SL099";
const char *FileName = "doubleLoggerFile.csv";
// Your logger's timezone.
const int TIME_ZONE = -5;
// Create TWO new logger instances
Logger logger1min;
Logger logger5min;
// Create a single modem instance
loggerModem modem;

// ==========================================================================
//    AOSong AM2315
// ==========================================================================
#include <AOSongAM2315.h>
const int I2CPower = 22;  // switched sensor power is pin 22 on Mayfly
AOSongAM2315 am2315(I2CPower);

// ==========================================================================
//    EnviroDIY Mayfly
// ==========================================================================
#include <MayflyOnboardSensors.h>
const char *MFVersion = "v0.3";
EnviroDIYMayfly mayfly(MFVersion) ;

// ---------------------------------------------------------------------------
// The two array that contains the variables for the different intervals
// ---------------------------------------------------------------------------
Variable *variableList_at1min[] = {
    new AOSongAM2315_Humidity(&am2315),
    new AOSongAM2315_Temp(&am2315)
    // new YOUR_variableName_HERE(&)
};
int variableCount1min = sizeof(variableList_at1min) / sizeof(variableList_at1min[0]);
Variable *variableList_at5min[] = {
    new EnviroDIYMayfly_Temp(&mayfly),
    new EnviroDIYMayfly_Batt(&mayfly),
    new EnviroDIYMayfly_FreeRam(&mayfly)
    // new YOUR_variableName_HERE(&)
};
int variableCount5min = sizeof(variableList_at5min) / sizeof(variableList_at5min[0]);


// ---------------------------------------------------------------------------
// Device Connection Options and WebSDL Endpoints for POST requests
// ---------------------------------------------------------------------------
const int modemDTRPin = 23;  // Modem DTR Pin (Data Terminal Ready - used for sleep) (-1 if unconnected)
const int modemCTSPin = 19;   // Modem CTS Pin (Clear to Send) (-1 if unconnected)
const int modemVCCPin = -1;  // Modem power pin, if it can be turned on or off (else -1)

DTRSleepType ModemSleepMode = reverse;  // How the modem is put to sleep
// Use "held" if the DTR pin is held HIGH to keep the modem awake, as with a Sodaq GPRSBee rev6.
// Use "pulsed" if the DTR pin is pulsed high and then low to wake the modem up, as with an Adafruit Fona or Sodaq GPRSBee rev4.
// Use "reverse" if the DTR pin is held LOW to keep the modem awake, as with all XBees.
// Use "always_on" if you do not want the library to control the modem power and sleep or if none of the above apply.
HardwareSerial &ModemSerial = Serial1; // The serial port for the modem - software serial can also be used.

const long ModemBaud = 9600;  // Default for XBee is 9600
const char *SSID = "XXXXXXX";  // The WiFi access point
const char *PWD = "XXXXXXX";  // The password for connecting to WiFi


// ---------------------------------------------------------------------------
// Board setup info
// ---------------------------------------------------------------------------
const long SERIAL_BAUD = 57600;  // Serial port baud rate
const int GREEN_LED = 8;  // Pin for the green LED
const int RED_LED = 9;  // Pin for the red LED
const int RTC_PIN = A7;  // RTC Interrupt/Alarm pin
const int SD_SS_PIN = 12;  // SD Card Chip Select/Slave Select Pin


// ---------------------------------------------------------------------------
// Working Functions
// ---------------------------------------------------------------------------

// Flashes to Mayfly's LED's
void greenred4flash()
{
  for (int i = 1; i <= 4; i++) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    delay(50);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    delay(50);
  }
  digitalWrite(RED_LED, LOW);
}


// ---------------------------------------------------------------------------
// Main setup function
// ---------------------------------------------------------------------------
void setup()
{
    // Start the primary serial connection
    Serial.begin(SERIAL_BAUD);
    // Start the serial connection with the *bee
    ModemSerial.begin(ModemBaud);

    // Set up pins for the LED's
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    // Blink the LEDs to show the board is on and starting up
    greenred4flash();

    // Start the Real Time Clock
    rtc.begin();
    delay(100);

    // Print a start-up note to the first serial port
    Serial.print(F("Now running "));
    Serial.print(SKETCH_NAME);
    Serial.print(F(" on Logger "));
    Serial.println(LoggerID);

    // Set the timezone and offsets
    Logger::setTimeZone(TIME_ZONE);  // Logging in the given time zone
    Logger::setTZOffset(TIME_ZONE);  // Set the clock in UTC

    // Initialize the two logger instances;
    logger1min.init(SD_SS_PIN, RTC_PIN, variableCount1min, variableList_at1min,
                1, LoggerID);
    logger5min.init(SD_SS_PIN, RTC_PIN, variableCount5min, variableList_at5min,
                5, LoggerID);
    // There is no reason to call the setAlertPin() function, because we have to
    // write the loop on our own.

    modem.setupModem(&ModemSerial, modemVCCPin, modemCTSPin, modemDTRPin, ModemSleepMode, SSID, PWD);

    // Set up the sensors on both loggers
    logger1min.setupSensors();
    logger5min.setupSensors();

    // Tell both loggers to save data to the same file
    // If we wanted to auto-generate the file name, that could also be done by
    // not calling this function.  If both "loggers" have the same logger id,
    // they will end up with the same filename
    logger1min.setFileName(FileName);
    logger5min.setFileName(FileName);

    // Setup the logger file
    // Because both loggers are saving to the same file, only
    // need to do this once
    logger1min.setupLogFile();
    // Create a header for the second logger and write it to the SD card
    logger5min.logToSD(logger5min.generateFileHeader());

    // Print out the current time
    Serial.print(F("Current RTC time is: "));
    Serial.println(Logger::formatDateTime_ISO8601(Logger::getNow()));

    // Turn on the modem
    modem.on();
    // Connect to the network
    if (modem.connectNetwork())
    {
        // Synchronize the RTC
        modem.syncDS3231();
        // Disconnect from the network
        modem.disconnectNetwork();
    }
    // Turn off the modem
    modem.off();

    // Set up the processor sleep mode
    // Because there's only one processor, we only need to do this once
    logger1min.setupSleep();

    Serial.println(F("Logger setup finished!\n"));
    Serial.println(F("------------------------------------------"));
    Serial.println();
}


// ---------------------------------------------------------------------------
// Main loop function
// ---------------------------------------------------------------------------

// Because of the way the sleep mode is set up, the processor will wake up
// and start the loop every minute exactly on the minute.
// Log the data
void loop()
{
    // Check if the current time is an even interval of the logging interval
    // For whichever logger we call first, use the checkInterval() function.
    if (logger1min.checkInterval())
    {
        // Print a line to show new reading
        Serial.println(F("--------------------->111<---------------------"));
        // Turn on the LED to show we're taking a reading
        digitalWrite(GREEN_LED, HIGH);

        // Wake up all of the sensors
        logger1min.sensorsWake();
        // Update the values from all attached sensors
        logger1min.updateAllSensors();
        // Immediately put sensors to sleep to save power
        logger1min.sensorsSleep();

        // Create a csv data record and save it to the log file
        logger1min.logToSD(logger1min.generateSensorDataCSV());

        // Turn off the LED
        digitalWrite(GREEN_LED, LOW);
        // Print a line to show reading ended
        Serial.println(F("---------------------<111>---------------------\n"));
    }
    // Check if the already marked time is an even interval of the logging interval
    // For logger[s] other than the first one, use the checkMarkedInterval() function.
    if (logger5min.checkMarkedInterval())
    {
        // Print a line to show new reading
        Serial.println(F("--------------------->555<---------------------"));
        // Turn on the LED to show we're taking a reading
        digitalWrite(RED_LED, HIGH);

        // Wake up all of the sensors
        logger5min.sensorsWake();
        // Update the values from all attached sensors
        logger5min.updateAllSensors();
        // Immediately put sensors to sleep to save power
        logger5min.sensorsSleep();

        // Create a csv data record and save it to the log file
        logger5min.logToSD(logger5min.generateSensorDataCSV());

        // Turn off the LED
        digitalWrite(RED_LED, LOW);
        // Print a line to show reading ended
        Serial.println(F("--------------------<555>---------------------\n"));
    }
    // Once a day, at midnight, sync the clock
    if (Logger::markedEpochTime % 86400 == 0)
    {
        // Turn on the modem
        modem.on();
        // Connect to the network
        if (modem.connectNetwork())
        {
            // Synchronize the RTC
            modem.syncDS3231();
            // Disconnect from the network
            modem.disconnectNetwork();
        }
        // Turn off the modem
        modem.off();
    }

    // Call the processor sleep
    // Only need to do this for one of the loggers
    logger1min.systemSleep();
}
