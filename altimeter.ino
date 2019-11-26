/* Diving Altimeter
-----------------------------------------------------
 
Title: Diving Altimeter

Description: Displays (LCD) and logs (SD card) depth 
and altitude. Designed for Arduino Uno with Adafruit
Datalogger Shield and BlueRobotics Ping & Bar30 
sensors.

-------------------------------*/
// Libraries
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <MS5837.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include "RTClib.h"
#include "ping1d.h"
#include "SoftwareSerial.h"

// Constants
#define chipSelect 8
#define arduinoRxPin 9
#define arduinoTxPin 10
#define LCD_COLS 20
#define LCD_ROWS 4
#define FLUID_DENSITY 1029

// Global objects/variables
File logfile;
SoftwareSerial pingSerial = SoftwareSerial(arduinoRxPin, arduinoTxPin);
hd44780_I2Cexp lcd;
MS5837 depth_sensor;
RTC_PCF8523 rtc;
static Ping1D ping { pingSerial };

// Initialize LCD screen
void init_lcd() {
  int lcd_status;
  lcd_status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(lcd_status) {
    // // non-zero status means begin() failed
    lcd_status = -lcd_status; // convert negative status value to positive number
    hd44780::fatalError(lcd_status); // error code using the onboard LED if possible & exit
  }
  // Clear screen
  lcd.clear();
}

// Initialize Real-Time Clock
void init_rtc() {
  lcd.setCursor(0, 0);
  if (! rtc.begin()) {
    lcd.print(F("Real-time clock...NO"));
  }
  // Set time to current computer time (at compile)
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  lcd.print(F("Real-time clock...OK"));
}

// Initialize depth sensor
void init_depth_sensor() {
  lcd.setCursor(0, 1);
  Wire.begin();
  // Initialize pressure depth_sensor (true if initialization was successful)
  while (!depth_sensor.init()) {
    lcd.print(F("Depth sensor...   NO"));
    delay(3000);
  }
  depth_sensor.setModel(MS5837::MS5837_30BA);
  depth_sensor.setFluidDensity(FLUID_DENSITY);
  lcd.print(F("Depth sensor...   OK"));
}

// Initialize altimeter
void init_sonar() {
  pingSerial.begin(9600);
  while (!ping.initialize()) {
    lcd.print(F("Sonar...          NO"));
    delay(3000);
  }
  lcd.setCursor(0, 2);
  lcd.print(F("Sonar...          OK"));
}

// Initialize SD card
void init_sdcard() {
  lcd.setCursor(0, 3);
  pinMode(chipSelect, OUTPUT);
  if (!SD.begin(chipSelect)) {
    lcd.print(F("SD card...        NO"));
  }
  // create a new file
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }
  if (! logfile) {
    lcd.print("SD card...        NO");
  } else {
    // print logfile header
    logfile.println(F("date,time,unixtime,depth,altitude,confidence,bottom_depth"));
    lcd.print(F("SD card: "));
    lcd.print(filename);
  }
}

// Update time on LCD
void lcd_update_time(DateTime now) {
  lcd.setCursor(12, 3);
  lcd.print(F("      "));
  lcd.setCursor(12, 3);
  if (now.hour() < 10) {
    lcd.print(F(" "));
  }
  lcd.print(now.hour(), DEC);
  lcd.setCursor(14, 3);
  lcd.print(F(":"));
  lcd.setCursor(15, 3);
  if (now.minute() < 10) {
    lcd.print(F("0"));
  }
  lcd.print(now.minute(), DEC);
  lcd.setCursor(17, 3);
  lcd.print(F(":"));
  lcd.setCursor(18, 3);
  if (now.second() < 10) {
    lcd.print(F("0"));
  }
  lcd.print(now.second(), DEC);
}

// Update sensor readings on LCD
void lcd_update_sensors(float depth, float altitude, float confidence, 
                        float bottom_depth, float voltage) {
  int conf_int;
  // Depth
  lcd.setCursor(0, 0);
  lcd.print(F("DPT:"));
  if (depth < 10.0) {
    lcd.print(F(" "));
  }
  lcd.print(depth);
  lcd.print(F("m"));
  // Ping voltage
  lcd.setCursor(15, 0);
  lcd.print(voltage);
  lcd.setCursor(19, 0);
  lcd.print(F("V"));
  // Altitude
  lcd.setCursor(0, 1);
  lcd.print(F("ALT:"));
  if (altitude < 10.0) {
    lcd.print(F(" "));
  }
  lcd.print(altitude);
  lcd.print(F("m"));
  // Altitude confidence
  lcd.setCursor(15, 1);
  lcd.print(F("("));
  conf_int = (int) confidence;
  lcd.print(conf_int);
  lcd.print(F(")   "));
  // Bottom
  lcd.setCursor(0, 3);
  lcd.print(F("BOT:"));
  if (bottom_depth < 10.0) {
    lcd.print(F(" "));
  }
  lcd.print(bottom_depth);
  lcd.print(F("m"));
}

// Log sensor readings to SD card
void log_sensor_readings(DateTime now, float depth, float altitude, 
                         float confidence, float bottom_depth) {
  logfile.print(now.year(), DEC);
  logfile.print(F("/"));
  logfile.print(now.month(), DEC);
  logfile.print(F("/"));
  logfile.print(now.day(), DEC);
  logfile.print(F(","));
  logfile.print(now.hour(), DEC);
  logfile.print(F(":"));
  logfile.print(now.minute(), DEC);
  logfile.print(F(":"));
  logfile.print(now.second(), DEC);
  logfile.print(F(","));
  logfile.print(now.unixtime());
  logfile.print(F(","));
  logfile.print(depth);
  logfile.print(F(","));
  logfile.print(altitude);
  logfile.print(F(","));
  logfile.print(confidence);
  logfile.print(F(","));
  logfile.print(bottom_depth);
  logfile.println();
  logfile.flush();
}

void setup() {
  // Initialize LCD screen object
  init_lcd();
  // Initialize Real-Time Clock
  init_rtc();
  // Initialize depth sensor
  init_depth_sensor();
  // Initialize altimeter sensor
  init_sonar();
  // Initialize SD card and logfile
  //init_sdcard();
  delay(2000);
  lcd.clear();
}

void loop() {
  float bottom_depth;
  float altitude;
  float confidence;
  float voltage;
  // Get time from RTC
  DateTime now = rtc.now();
  lcd_update_time(now);
   // Get depth reading
  depth_sensor.read();
  // Get altitude reading
    if (ping.update()) {
        altitude = (float)ping.distance() / 1000;
        confidence = ping.confidence();
    } else {
        altitude = 0.0;
        confidence = 0.0;
    }
  voltage = (float)ping.voltage_5() / 1000;
  // Display sensor readings
  bottom_depth = depth_sensor.depth() + altitude;
  lcd_update_sensors(depth_sensor.depth(), altitude, confidence, bottom_depth, voltage);
  // Log sensor readings
  log_sensor_readings(now, depth_sensor.depth(), altitude, confidence, bottom_depth); 
  delay(1000);
}
