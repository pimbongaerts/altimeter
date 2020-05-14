/* Diving Altimeter
-----------------------------------------------------
 
Title: Aratui TEST

Description: 
-------------------------------*/
// Libraries
#include <Wire.h>                          // I2C Communication
#include <Adafruit_GFX.h>                  // Graphics for OLED
#include <Adafruit_SSD1305.h>              // OLED
#include <RTClib.h>                        // Real-time clock
#include <SPI.h>                           // SPI for SD card
#include <SD.h>                            // SD card reader
#include <MS5837.h>                        // Bar30 depth sensor
#include <ping1d.h>                        // Ping sonar / altimeter
#include <Bounce2.h>                       // Debouncing remote buttons

// Constants
#define OLED_COLS 20
#define OLED_ROWS 4
#define FLUID_DENSITY 1029
#define BUTTON_PRESS_DELAY 300
#define NOT_RESPONSIVE -999
#define NOT_RUNNING -999
#define INTERVAL 1000
#define SHUTTER_DELAY 300

// #define PIN_LED_GREEN 35               // Removed from board
// #define PIN_LED_RED 36                 // Removed from board
#define PIN_CAM_SHUTTER 40
#define PIN_CAM_FOCUS 41
#define PIN_BUTTON1 47
#define PIN_BUTTON2 48
#define PIN_BUTTON3 49
#define PIN_CHIPSELECT 53
#define OLED_RESET 70

// Global objects
File logfile;
Adafruit_SSD1305 display(OLED_RESET);
RTC_PCF8523 rtc;
MS5837 depth_sensor;
static Ping1D ping_left { Serial1 };
static Ping1D ping_right { Serial2 };

// Global objects
unsigned long stopwatch_stored_millis;
unsigned long stopwatch_start_millis;
unsigned long stopwatch_millis;
unsigned long previous_millis = 0;
int stopwatch_min = 0;
int stopwatch_sec = 0;
bool stopwatch_running = false;
bool delayed_start_stopwatch = false;
bool delayed_single_trigger_camera = false;
float ping_altitude_left = NOT_RESPONSIVE;
float ping_altitude_conf_left = NOT_RESPONSIVE;
float ping_altitude_right = NOT_RESPONSIVE;
float ping_altitude_conf_right = NOT_RESPONSIVE;
float mean_altitude = NOT_RESPONSIVE;
float bar30_depth = NOT_RESPONSIVE;
float extrapolated_depth = NOT_RESPONSIVE;
float usbl = NOT_RESPONSIVE;
int rtc_year = NOT_RESPONSIVE;
int rtc_month = NOT_RESPONSIVE;
int rtc_day = NOT_RESPONSIVE;
int rtc_hour = NOT_RESPONSIVE;
int rtc_min = NOT_RESPONSIVE;
int rtc_sec = NOT_RESPONSIVE;
unsigned long rtc_unixtime = NOT_RESPONSIVE;
int sd_file_number = NOT_RESPONSIVE;
int progress_update = -1;
int intervalometer_number = NOT_RUNNING;
int total_trigger_number = 0;
int current_menu = 0;
byte customBackslash[8] = {
  0b00000,
  0b10000,
  0b01000,
  0b00100,
  0b00010,
  0b00001,
  0b00000,
  0b00000
};

void init_pins() {
  // Initialize pins
  pinMode(PIN_BUTTON1, INPUT);
  pinMode(PIN_BUTTON2, INPUT);
  pinMode(PIN_BUTTON3, INPUT);
  pinMode(PIN_CAM_SHUTTER, OUTPUT);
  pinMode(PIN_CAM_FOCUS, OUTPUT);
}

// Initialize LCD screen
void init_oled() {
  display.begin();
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("DEEPCAT");
  display.display();
}

// Initialize Real-Time Clock
void init_rtc() {
  if (! rtc.begin()) {
    rtc_hour = NOT_RESPONSIVE;
  } else {
    // SET TIME TO CURRENT COMPUTER TIME
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // Update current date/time
    get_rtc_time();
  }
}

void get_rtc_time() {
  DateTime now = rtc.now();
  rtc_year = now.year();
  rtc_month = now.month();
  rtc_day = now.day();
  rtc_hour = now.hour();
  rtc_min = now.minute();
  rtc_sec = now.second();
  rtc_unixtime = now.unixtime();
}

// Initialize SD card
void init_sdcard() {
  display.clearDisplay();
  pinMode(PIN_CHIPSELECT, OUTPUT);
  if (!SD.begin(PIN_CHIPSELECT)) {
    sd_file_number = NOT_RESPONSIVE;
  }
  // create a new file
  char filename[] = "ARAT0000.CSV";
  for (uint8_t i = 0; i < 10000; i++) {
    filename[4] = '0' + ((i / 1000) % 10);
    filename[5] = '0' + ((i / 100) % 10);
    filename[6] = '0' + ((i / 10) % 10);
    filename[7] = '0' + (i % 10);
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE);
      sd_file_number = i;
      break;  // leave the loop!
    }
  }
  if (! logfile) {
    sd_file_number = NOT_RESPONSIVE;
  } else {
    // print logfile header
    logfile.println(F("date,time,unixtime,depth,left_altitude,left_altitude_confidence,right_altitude,right_altitude_confidence,mean_altitude,extrapolated_depth,stopwatch_time,intervalometer_no,total_trigger_number,millis"));
  }
}

void init_depth_sensor() {
  // Initialize pressure depth_sensor (true if initialization was successful)
  Wire.begin();
  while (!depth_sensor.init()) {
    delay(1000);
  }
  depth_sensor.setModel(MS5837::MS5837_30BA);
  depth_sensor.setFluidDensity(FLUID_DENSITY);
  get_bar30_depth();
}

// Obtain depth measurement
void get_bar30_depth() {
  depth_sensor.read();
  bar30_depth = depth_sensor.depth(); 
}

// Initialize left altimeter
void init_sonar_left() {
  Serial1.begin(9600);
  int attempts = 0;
  while (!ping_left.initialize()) { 
    if (attempts > 3) { break; }
    attempts++;
    delay(200);
  }
  ping_get_altitude_left();
}

// Initialize right altimeter
void init_sonar_right() {
  Serial2.begin(115200);
  int attempts = 0;
  while (!ping_right.initialize()) { 
    if (attempts > 10) { break; }
    attempts++;
    delay(500);
  }
  ping_left.set_ping_enable(true); // to remove
  ping_get_altitude_right();
}

// Initialize right altimeter
void init_usbl() {
  Serial3.begin(115200);
  if (Serial3.available() <= 0 ) {
    usbl = NOT_RESPONSIVE;
  }
}

// Obtain altitude measurement
void ping_get_altitude_left() {
    // ping_left.set_ping_enable(true);
    if (ping_left.update()) {
        ping_altitude_left = (float)ping_left.distance() / 1000;
        ping_altitude_conf_left = ping_left.confidence();
    } else {
        ping_altitude_left = NOT_RESPONSIVE;
        ping_altitude_conf_left = 0;
    }
    // ping_left.set_ping_enable(false);
}

// Obtain altitude measurement
void ping_get_altitude_right() {
    // ping_right.set_ping_enable(true);
    if (ping_right.update()) {
        ping_altitude_right = (float)ping_right.distance() / 1000;
        ping_altitude_conf_right = ping_right.confidence();
    } else {
        ping_altitude_right = NOT_RESPONSIVE;
        ping_altitude_conf_right = 0;
    }
    // ping_right.set_ping_enable(false);
}
// Obtain USBL information
void usbl_get_data() {
  if (Serial3.available() > 0) {
    usbl = 1;
  } else {
    usbl = NOT_RESPONSIVE;
  }
}

// Print depth/altitude to LCD
void oled_print_6char(float number) {
  if (number == NOT_RESPONSIVE) {
    display.print(F("-N/A- "));
  } else if (number >= 1000) {
    display.print(F("100+m "));
  } else if (number > 100) {
    display.print((int) number);
    display.print(F("m "));
  } else if (number > 10) {
    display.print(number);
    display.print(F("m"));
  } else if (number > 0) {
    display.print(F("0"));
    display.print(number);
    display.print(F("m"));
  } else if (number < 0) {
    display.print(F("-NEG- "));
  } else {
    display.print(F("ERROR "));
  }
}

// Print time to OLED
void oled_print_rtc_time(int number1, int number2, int number3) {
  if (number1 == NOT_RESPONSIVE) {
    display.print(F("-N/A-"));
  } else {
    if (number1 < 10) { display.print(F("0")); }
    display.print(number1);
    display.print(F(":"));
    if (number2 < 10) { display.print(F("0")); }
    display.print(number2);
    display.print(F(":"));
    if (number3 < 10) { display.print(F("0")); }
    display.print(number3);
  }
}

// Print time to OLED
void oled_print_time(int number1, int number2) {
  if (number1 == NOT_RESPONSIVE) {
    display.print(F("-N/A-"));
  } else {
    if (number1 < 10) { display.print(F("0")); }
    display.print(number1);
    display.print(F(":"));
    if (number2 < 10) { display.print(F("0")); }
    display.print(number2);
  }
}

// Print SD card filenumber to OLED
void oled_print_sd_card() {
  if (sd_file_number == NOT_RESPONSIVE) {
    display.print(F("NOSD"));
    return;
  } else if (sd_file_number < 10) {
    display.print(F("SD0"));
  } else if (sd_file_number < 100) {
    display.print(F("SD"));
  } else if (sd_file_number < 1000) {
    display.print(F("0"));
  }
  display.print(sd_file_number);
}

// Print photo number to OLED
void oled_print_total_trigger_number() {
  if (total_trigger_number < 10) {
    display.print(F("000"));
  } else if (total_trigger_number < 100) {
    display.print(F("00"));
  } else if (total_trigger_number < 1000) {
    display.print(F("0"));
  }
  display.print(total_trigger_number);
}

// Get mean altitude
void get_mean_altitude() {
  if ((ping_altitude_left != NOT_RESPONSIVE) && (ping_altitude_right != NOT_RESPONSIVE)) {
    mean_altitude = (ping_altitude_left + ping_altitude_right) / 2;
  } else if (ping_altitude_left != NOT_RESPONSIVE) {
    mean_altitude = ping_altitude_left;
  } else if (ping_altitude_right != NOT_RESPONSIVE) {
    mean_altitude = ping_altitude_right;
  } else {
    mean_altitude = NOT_RESPONSIVE;
  }
}

// Get extrapolated depth
void get_extrapolated_depth() {
  if (mean_altitude != NOT_RESPONSIVE) {
    extrapolated_depth = bar30_depth + mean_altitude;
  } else {
    extrapolated_depth = NOT_RESPONSIVE;
  }
}

// Get progress update character
void oled_print_progress_character() {
  display.setCursor(122, 17);
  if (intervalometer_number == NOT_RUNNING) {
    if (progress_update == 0) { 
      display.print(F("*")); 
    } else {
      display.print(F("."));
      progress_update = -1;
    }
  } else {
    if (progress_update == 0) { 
      display.print(F("|")); 
    } else if (progress_update == 1) { 
      display.print(F("/")); 
    } else if (progress_update == 2) { 
      display.print(F("-")); 
    } else {
      display.print(F("\\"));
      progress_update = -1;
    }
  }
}

// Get progress update character
void oled_print_button_character() {
  display.setCursor(122,17);
  display.print(F("B"));
  display.display();
}

// ALtitude levelling feedback
void oled_print_altitude_leveller() {
  char distance_char;
  // Determine correction for mean altitude
  if (mean_altitude == NOT_RESPONSIVE) {
    display.print(F("|--N/A--|"));
    return;  
  } else if ((mean_altitude >= 1.15) && (mean_altitude <= 1.35)) {
    distance_char = '*';
  } else if ((mean_altitude > 1.35) && (mean_altitude < 1.5)) {
    distance_char = 'c';
  } else if (mean_altitude >= 1.5) {
    distance_char = 'C';
  } else if ((mean_altitude < 1.15) && (mean_altitude > 1.0)) {
    distance_char = 'f';
  } else if (mean_altitude <= 1.0) {
    distance_char = 'F';
  } else {
    distance_char = 'E';
  }
  // Determine correction to level
  float altitude_differential = ping_altitude_right - ping_altitude_left;
  if ((altitude_differential >= -0.10) && (altitude_differential <= 0.10)) {
    display.print(F("|---"));
    display.print(distance_char);
    display.print(F("---|"));
  } else if ((altitude_differential < -0.10) && (altitude_differential >= -0.20)) {
    display.print(F("|--"));
    display.print(distance_char);
    display.print(F("----|"));  
  } else if ((altitude_differential < -0.20) && (altitude_differential >= -0.40)) {
    display.print(F("|-"));
    display.print(distance_char);
    display.print(F("-----|"));
  } else if (altitude_differential < -0.40) {
    display.print(F("|"));
    display.print(distance_char);
    display.print(F("------|"));
  } else if ((altitude_differential > 0.10) && (altitude_differential <= 0.20)) {
    display.print(F("|----"));
    display.print(distance_char);
    display.print(F("--|"));
  } else if ((altitude_differential > 0.20) && (altitude_differential <= 0.40)) {
    display.print(F("|-----"));
    display.print(distance_char);
    display.print(F("-|"));
  } else if (altitude_differential > 0.40) {
    display.print(F("|------"));
    display.print(distance_char);
    display.print(F("|"));
  } else {
    display.print(F("|---E---|"));   
  }
}

// ALtitude levelling feedback
void oled_print_usbl() {
  if (usbl == 1){
    display.print(F("USBL  "));
  } else if (usbl == NOT_RESPONSIVE) {
    display.print(F("noGPS "));
  }
}

// Update home screen
void update_oled_home() {
  display.clearDisplay();
  // FIRST LINE OF LCD
  display.setCursor(0, 0); 
  if ((current_menu == 0) or (current_menu == 1)) {
    // Altitude left
    oled_print_6char(ping_altitude_left);
    // Altitude levelling feedback
    oled_print_altitude_leveller();
    // Altitude left
    oled_print_6char(ping_altitude_right);
    // SECOND LINE OF LCD
    display.setCursor(0, 9);
    // GPS
    oled_print_usbl();
    // Mean altitude
    display.print(F(" A:"));
    oled_print_6char(mean_altitude);
    // Altitude help right
    display.print(F(" "));
    oled_print_sd_card();
    // THIRD LINE OF LCD
    display.setCursor(0, 17);  
    // BAR30 depth
    oled_print_6char(bar30_depth);
    // Extrapolated depth
    display.print(F(" B:"));
    oled_print_6char(extrapolated_depth);
    // Total trigger number
    display.print(F(" "));
    oled_print_total_trigger_number();
    display.print(F(" "));
    oled_print_progress_character();
    // FOURTH LINE OF LCD
    display.setCursor(0, 25);
    if (current_menu == 0) {
      display.setTextColor(BLACK, WHITE);
      display.print(F("MENU   "));
      display.setTextColor(WHITE);
      // RTC Time (5 chars)
      oled_print_rtc_time(rtc_hour, rtc_min, rtc_sec);
      // Stopwatch (updated here to make as up-to-date as possible)
      get_stopwatch_time();
      display.print(F(" "));
      oled_print_time(stopwatch_min, stopwatch_sec);
    } else if (current_menu == 1) {
      display.setTextColor(BLACK, WHITE);
      display.print(F("MENU            FIRE"));
      display.setTextColor(WHITE);
    }
  } else if (current_menu == 2) {
    if (Serial3.available() > 0 ) {
      int char_count = 0;
      while (Serial3.available() > 0) {
        char t = Serial3.read();
        display.print(t);
        char_count += 1;
        if (char_count > 80) { break; }
      }
      display.setTextColor(BLACK, WHITE);
      display.print(F("                MENU"));
      display.setTextColor(WHITE);
    }
  }
  display.display();
}

// Check input for any button presses
void check_button_presses() {
  // Home menu
  if (current_menu == 0){
    if (digitalRead(PIN_BUTTON1) == LOW) {
      // Start intervalometer and continue stopwatch
      if (intervalometer_number == NOT_RUNNING) {
        intervalometer_number = 0;
        delayed_start_stopwatch = true;
      } else {
        intervalometer_number = NOT_RUNNING;
        stop_stopwatch();
      }
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    } else if (digitalRead(PIN_BUTTON2) == LOW) {
      // Reset stopwatch
      reset_stopwatch();
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    } else if (digitalRead(PIN_BUTTON3) == LOW) {
      // Change menu
      current_menu = 1;
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    }
  } else if (current_menu == 1){
    if (digitalRead(PIN_BUTTON1) == LOW) {
      // Set delayed camera trigger for single shot
      delayed_single_trigger_camera = true;
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    } else if (digitalRead(PIN_BUTTON3) == LOW) {
      // Change menu
      current_menu = 2;
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    }
  } else if (current_menu == 2){
    if (digitalRead(PIN_BUTTON3) == LOW) {
      // Change menu
      current_menu = 0;
      oled_print_button_character();
      delay(BUTTON_PRESS_DELAY);
    }
  }
}

// Start stopwatch
void start_stopwatch() {
  stopwatch_running = true;
  stopwatch_start_millis = millis();
  delayed_start_stopwatch = false;
}

// Get current stopwatch time
void get_stopwatch_time() {
  if (stopwatch_running) {
    stopwatch_millis = stopwatch_stored_millis + millis() - stopwatch_start_millis;
  } else {
    stopwatch_millis = stopwatch_stored_millis;
  }
  int stopwatch_total_secs = stopwatch_millis / 1000;
  stopwatch_min = stopwatch_total_secs / 60;
  stopwatch_sec = stopwatch_total_secs - (stopwatch_min * 60);
}

// Start stopwatch
void stop_stopwatch() {
  get_stopwatch_time();
  stopwatch_running = false;
  stopwatch_stored_millis = stopwatch_millis;
}

// Reset stopwatch
void reset_stopwatch() {
  stopwatch_start_millis = millis();
  stopwatch_stored_millis = 0;
}

// Trigger camera shutter
void trigger_camera() {
  digitalWrite(PIN_CAM_SHUTTER, HIGH);
  intervalometer_number++;
  total_trigger_number++;
  delay(SHUTTER_DELAY);
  digitalWrite(PIN_CAM_SHUTTER, LOW);
}

// Trigger camera shutter
void single_trigger_camera() {
  digitalWrite(PIN_CAM_SHUTTER, HIGH);
  total_trigger_number++;
  delay(SHUTTER_DELAY);
  digitalWrite(PIN_CAM_SHUTTER, LOW);
  delayed_single_trigger_camera = false;
}

// Log sensor readings to SD card
void log_sensor_readings() {
  logfile.print(rtc_year, DEC);           // 1. Date
  logfile.print(F("/"));
  logfile.print(rtc_month, DEC);
  logfile.print(F("/"));
  logfile.print(rtc_day, DEC);
  logfile.print(F(","));
  logfile.print(rtc_hour, DEC);           // 2. Time
  logfile.print(F(":"));
  logfile.print(rtc_min, DEC);
  logfile.print(F(":"));
  logfile.print(rtc_sec, DEC);
  logfile.print(F(","));
  logfile.print(rtc_unixtime);            // 3. Unix Time
  logfile.print(F(","));
  logfile.print(bar30_depth);             // 4. Bar30 Depth
  logfile.print(F(","));
  logfile.print(ping_altitude_left);      // 5. Left altitude
  logfile.print(F(","));
  logfile.print(ping_altitude_conf_left); // 6. Left altitude confidence
  logfile.print(F(","));
  logfile.print(ping_altitude_right);     // 7. Right altitude
  logfile.print(F(","));
  logfile.print(ping_altitude_conf_right);// 8. Right altitude confidence
  logfile.print(F(","));
  logfile.print(mean_altitude);           // 9. Mean altitude
  logfile.print(F(","));
  logfile.print(extrapolated_depth);      // 9. Extrapolated depth
  logfile.print(F(","));
  logfile.print(stopwatch_min);           // 10. Stopwatch time
  logfile.print(F(":"));
  logfile.print(stopwatch_sec);
  logfile.print(F(","));
  logfile.print(intervalometer_number);   // 11. Intervalometer number
  logfile.print(F(","));
  logfile.print(total_trigger_number);    // 12. Total trigger number
  logfile.print(F(","));
  logfile.print(millis());    // 13. millis
  logfile.println();
  logfile.flush();
}

void setup() {
  // Initialize pins
  init_pins();
  // Initialize LCD screen
  init_oled();
  // Initialize Real-Time Clock
  init_rtc();
  // Initialize SD card and logfile
  init_sdcard();
  update_oled_home();
  // Initialize depth sensor
  init_depth_sensor();
  update_oled_home();
  // Initialize left altimetr
  // init_sonar_left();
  update_oled_home();
  // Initialize rightaltimetr
  init_sonar_right();
  update_oled_home();
  // Initialize USBL
  //init_usbl();
  // Show on screen
  update_oled_home();
}

void loop() {
  // Check for button presses
  check_button_presses();
  // Update sensor readings
  get_rtc_time();
  get_bar30_depth();
  // ping_get_altitude_left();
  ping_get_altitude_right();
  get_mean_altitude();
  get_extrapolated_depth();
  //usbl_get_data();
  // Single camera shot if triggered
  if (delayed_single_trigger_camera) {
      single_trigger_camera();
  }
  if (millis() - previous_millis >= INTERVAL ){
    previous_millis = millis();
    // Start stopwatch if it has been triggered
    if (delayed_start_stopwatch) { start_stopwatch(); }
    // Trigger camera if intervalometer is on
    if (intervalometer_number != NOT_RUNNING) { trigger_camera(); }
    // Log readings
    log_sensor_readings();
    // Progress update
    progress_update++;
  }
  // Update screen
  delay(300); // to remove
  update_oled_home();
}
