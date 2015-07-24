#include <TFTv2.h>

// Smart Alarm Clock
// Copyright 2014 Tony DiCola (tony@tonydicola.com)
// Released under an MIT license (http://opensource.org/licenses/MIT)

#include <SPI.h>

#include <SeeedTouchScreen.h>


#include <Process.h>

// Functionality:
//   Sets alarm based on earliest event in google calendar.
//   Refresh button forces an update with google calendar.
//   Alarm is updated every hour.
//   Cancel button cancels a scheduled alarm and stops alarm checks for the next 24 hours.  Cancel the cancel by clicking refresh.
//   After alarm is stopped, alarms are disabled for next 8 hours.
//   Only meetings before noon will trigger the alarm.

// Temboo & Google Calendar/GMail configuration.

// Temboo account username:
#define TEMBOO_ACCOUNT      "bcopeland"
// Temboo account app name:
#define TEMBOO_APP          "smartAlarm"
// Temboo account app key:
#define TEMBOO_KEY          "b663da47d28741b9986363324f508f98"
// Temboo Google calendar credential name:
#define CALENDAR_CREDENTIALS  "googleCalendar"
// Google calendar ID:
#define CALENDAR_ID         "brooklyn.copeland98@gmail.com"
// Temboo Gmail credential name:
#define GMAIL_CREDENTIALS   "GoogleMail"
// Mail subject keyword to trigger alarm:
#define GMAIL_ALARM_KEYWORD  "WAKE UP"


// Alarm configuration.
// Name of the alarm MP3.
#define ALARM_FILE        "/mnt/sd/arduino/www/mySmartAlarmClock/GoodMorning.mp3"
// Options to specify when running madplay command.
// The --attenuate command controls the volume of the alarm (in decibels).  A negative value will reduce the
// volume, and a positive value will increase the volume.
#define ALARM_OPTIONS     "--attenuate=-12"
// Minutes to wait between calendar refreshes:
#define ALARM_REFRESH_MINS  5
// Meetings after this hour (0-23) will be ignored.  Set to 24 to disable this behavior.
#define ALARM_LATEST_HOUR   24
// Amount of time before the first meeting to fire the alarm.
#define ALARM_BUFFER_MINS   5
// Minutes to wait before checking for new wake up emails.
#define ALARM_KEYWORD_MINS  120

// Touchscreen calibration.
// You don't need to change these values.
/*
#define TS_MINX 140
#define TS_MAXX 900
#define TS_MINY 120
#define TS_MAXY 940
*/
// Clock display configuration.
// You don't need to change these values.
#define CLOCK_COLOR        CYAN
#define CLOCK_CENTER_X     MAX_X/2 - 20
#define CLOCK_CENTER_Y     MAX_Y/4 + 20
#define CLOCK_RADIUS       75
#define CLOCK_TICK_SIZE    7.5
#define REFRESH_X          20
#define REFRESH_Y          250
#define CANCEL_X           230
#define CANCEL_Y        210
#define BUTTON_WIDTH    25
#define BUTTON_HEIGHT   85


// Internal state of the alarm clock.
unsigned long lastUpdate = 0;
int time = 0;
int alarmTime = -1;
Process alarmPlayback;
int alarmNextCheck;
bool alarmPlaying = false;
TouchScreen ts = TouchScreen(XP, YP, XM, YM);
unsigned long mailLastSeen = 0;
int setHour = 1;
int setMin = 1;
int alarmHour =1;
int alarmMin=1;

// Setup function, called once to initialize alarm state.
void setup()
{
  // Start the bridge and serial port.
  Bridge.begin();
  Serial.begin(9600);
  // Initialize display and set orientation.
  Tft.TFTinit();

  // Display a loading message.
  Tft.drawString("LOADING...", 20, 10, 3, CLOCK_COLOR);
  // Update the time (stored as minute of the day, i.e. a value from 0 to 1440 (24*60)).
  time = get_time();
  // Get time of last seen mail with alarm keyword.
  mailLastSeen = get_mail_lastseen();
  // Update next alarm time.
  alarmTime = get_alarm();
  // Draw the display.
  draw_display(time, alarmTime);
  // Keep track of when the display was last updated.
  lastUpdate = millis();
}

// Loop function called continuousl after setup.
void loop() //------------------------------------------------------------------------------
{
  // Grab current milliseconds.
  unsigned long current = millis();

  // Update the clock after a minute has passed since last update.
  if (long(current - lastUpdate) >= 60000L) {

    // Increase the time by a minute.
    time = time_add(time, 1);
    // Update the time every day at midnight to fix any drift.
    if (time == 0) {
      time = get_time();
    }
    // Note there is the chance that a time update might move the time past an alarm or
    // alarm update.  Fixing this is correctly is not easy without introducing more timestamp
    // manipulation, so it's left as an uncommon but known issue.

    // Sound alarm if wake up mail has been received.
    if (time % ALARM_KEYWORD_MINS == 0) {
      check_mail_alarm();
    }
    // Update alarm time periodically.
    if (time == alarmNextCheck) {
      alarmTime = get_alarm();
    }
    // Start alarm if necessary.
    if (time == alarmTime) {
      alarm_start();
    }
    if((time/60)==alarmHour && (time%60)==alarmMin)
    {
      alarm_start();
      }
    // Update display.
    draw_display(time, alarmTime);
    // Remember last update time.
    lastUpdate = millis();
  }

  // Check for touchscreen hit.
  Point p = ts.getPoint();
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

  // If the alarm is playing and the touchscreen is pressed anywhere, stop the alarm.
    if (p.z > __PRESURE && alarmPlaying) {
      // Stop the alarm.
      alarm_stop();
      // Remove the current alarm and stop alarm checks for the next 8 hours.
      //alarmTime = -1;
      //alarmNextCheck = time_add(time, 8*60);
      // Redraw the display.
      draw_display(time, alarmTime);
  }



  // Check for refresh button hit.
  if (p.z >  __PRESURE && p.x >= 20 && p.x <= 105 && p.y >= 250 && p.y <= 275) {
    // Highlight the button so it's clear the refresh is happening.
    Tft.fillRectangle(REFRESH_X, REFRESH_Y, BUTTON_HEIGHT, BUTTON_WIDTH, CLOCK_COLOR);
    Tft.drawString("REFRSH", REFRESH_X + 30, REFRESH_Y + 5, 2, BLACK);
    // Update time.
    time = get_time();
    // Update alarm time.
    alarmTime = get_alarm();
    // Check for any mail alarm.
    check_mail_alarm();
    // Redraw the display.
    draw_display(time, alarmTime);
  }

  // check for test button hit
  if (p.z >  __PRESURE && p.x >= 150 && p.x <= 200 && p.y >= 250 && p.y <= 275) {
    alarm_start();
    delay(3000);
    alarm_stop();
  }

  //check for set button hit
  if (p.z >  __PRESURE && p.x >= 100 && p.x <= 140 && p.y >= 200 && p.y <= 220) {
    draw_set_display();
  }

  // check for increase hour hit
  if (p.z >  __PRESURE && p.x >= 25 && p.x <= 75 && p.y >= 20 && p.y <= 70) {
    setHour++;
    if (setHour > 12)
    {
      setHour = 1;
    }
    draw_set_display();
  }

  //check for decrease hour hit
  if (p.z >  __PRESURE && p.x >= 25 && p.x <= 75 && p.y >= 125 && p.y <= 175) {
    setHour--;
    if (setHour < 1)
    {
      setHour = 12;
    }
    draw_set_display();
  }

  //check for increase min hit
  if (p.z >  __PRESURE && p.x >= 125 && p.x <= 175 && p.y >= 20 && p.y <= 70) {
    setMin++;
    if (setMin > 60)
    {
      setMin = 1;
    }
    draw_set_display();
  }

  //check for decrease min hit
  if (p.z >  __PRESURE && p.x >= 125 && p.x <= 175 && p.y >= 125 && p.y <= 175) {
    setMin--;
    if (setMin < 1)
    {
      setMin = 60;
    }
    draw_set_display();
  }

  //check for ok button hit
  if (p.z >  __PRESURE && p.x >= 185 && p.x <= 225 && p.y >= 85 && p.y <= 110) {
    alarmHour=setHour;
    alarmMin=setMin;
    Tft.fillRectangle(0, 0, 240, 320, BLACK);
    Tft.drawString("ALARM SET", 20, 10, 3, CLOCK_COLOR);
    
  }

  /*
  // Check for cancel button hit.
  if (p.z > ts.pressureThreshhold && p.x >= CANCEL_X && p.x <= (CANCEL_X + BUTTON_HEIGHT) && p.y >= CANCEL_Y && p.y <= (CANCEL_Y + BUTTON_WIDTH)) {
    // Disable the alarm.
    alarmTime = -1;
    // Stop alarm updates for the next ~24 hours.
    alarmNextCheck = time_add(time, -1);
    // Redraw the display.
    draw_display(time, alarmTime);
  }
  */
  // Short delay to keep touchscreen checks responsive.
  delay(10);
}//-----------------------------------------------------------------------------------------------------------------------

// Start playing alarm audio.
void alarm_start() {
  // Kill the alarm if it's running.
  alarm_stop();
  // Start the alarm.
  alarmPlayback.begin("madplay");
  alarmPlayback.addParameter("-Qr"); // Disable output of status and repeat audio continuously.
  alarmPlayback.addParameter(ALARM_OPTIONS);
  alarmPlayback.addParameter(ALARM_FILE);
  alarmPlayback.runAsynchronously();
  alarmPlaying = true;
}

// Stop playing alarm audio.
void alarm_stop() {
  if (alarmPlayback.running()) {
    alarmPlayback.close();
    alarmPlaying = false;
  }
}

// Convert hour and minute to time string.  String MUST be a 9 character buffer!
void time_to_string(int time, char* timeString) {
  int hour12, minute;
  bool am;
  time_to_hour_minute(time, hour12, minute, am);
  timeString[8] = 0;
  timeString[7] = 'M';
  timeString[6] = am ? 'A' : 'P';
  timeString[5] = ' ';
  timeString[4] = '0' + (minute % 10);
  timeString[3] = '0' + (minute / 10);
  timeString[2] = ':';
  timeString[1] = '0' + (hour12 % 10);
  timeString[0] = (hour12 > 9) ? '1' : ' ';
}

// Add a specified number of minutes to the provided time and return the new time value (in minutes of the day).
int time_add(int time, int minutes) {
  int result = time + minutes;
  if (result < 0) {
    return 1440 - ((-1 * result) % 1440);
  }
  else {
    return result % 1440;
  }
}

// Convert from hour (24 hour format) and minute to minutes in day.
int time_to_minutes(int hour24, int minute) {
  return hour24 * 60 + minute;
}

// Convert from minutes in day to hour (12 hour format), minute, and AM/PM.
int time_to_hour_minute(int time, int& hour12, int& minute, bool& am) {
  hour12 = time / 60;
  am = true;
  if (hour12 > 12) {
    hour12 -= 12;
    am = false;
  }
  minute = time % 60;
}

// Run date command to get the current hour and minute from the Yun's Linux processor.
int get_time() {
  Process date;
  date.begin("date");
  date.addParameter("+%H%M");
  date.run();
  int hour24 = 10 * (date.read() - '0');
  hour24 += date.read() - '0';
  int minute = 10 * (date.read() - '0');
  minute += date.read() - '0';
  return time_to_minutes(hour24, minute);
}

// Run the find_alarm.py script to search google calendar for the first alarm in the next 24 hours.
int get_alarm() {
  Process findAlarm;
  findAlarm.begin("python");
  findAlarm.addParameter("/mnt/sd/arduino/www/mySmartAlarmClock/find_alarm.py");
  findAlarm.addParameter(TEMBOO_ACCOUNT);
  findAlarm.addParameter(TEMBOO_APP);
  findAlarm.addParameter(TEMBOO_KEY);
  findAlarm.addParameter(CALENDAR_CREDENTIALS);
  findAlarm.addParameter(CALENDAR_ID);
  findAlarm.run();
  // Check if an alarm was found and parse the time.
  int alarm = -1;
  if (findAlarm.available() >= 2) {
    int alarmHour24 = int(findAlarm.read());
    int alarmMinute = int(findAlarm.read());
    alarm = time_to_minutes(alarmHour24, alarmMinute);
    // Only update alarm if it's earlier than the latest alarm hour.
    if (alarm < (ALARM_LATEST_HOUR * 60)) {
      // Adjust alarm based on buffer minutes.
      alarm = time_add(alarm, -ALARM_BUFFER_MINS);
    }
    else {
      // Alarm is too late in the day to fire.
      alarm = -1;
    }
  }
  alarmNextCheck = time_add(time, ALARM_REFRESH_MINS);
  return alarm;
}

// Run the check_mail.py script to check email for most recent unread message with keyword in the title.
unsigned long get_mail_lastseen() {
  Process checkMail;
  checkMail.begin("python");
  checkMail.addParameter("/mnt/sd/arduino/www/mySmartAlarmClock/check_email.py");
  checkMail.addParameter(TEMBOO_ACCOUNT);
  checkMail.addParameter(TEMBOO_APP);
  checkMail.addParameter(TEMBOO_KEY);
  checkMail.addParameter(GMAIL_CREDENTIALS);
  checkMail.addParameter(GMAIL_ALARM_KEYWORD);
  checkMail.run();
  // Check if an alarm was found and parse the time.
  unsigned long lastseen = 0;
  // Read unsigned long response from result.
  if (checkMail.available() >= 4) {
    lastseen |= ((unsigned long)checkMail.read() << 24);
    lastseen |= ((unsigned long)checkMail.read() << 16);
    lastseen |= ((unsigned long)checkMail.read() << 8);
    lastseen |= checkMail.read();
  }
  return lastseen;
}

// Sound the alarm if an email has been received which contains the alarm keyword.
void check_mail_alarm() {
  unsigned long lastSeen = get_mail_lastseen();
  if (lastSeen > mailLastSeen) {
    // Found a new mail, sound the alarm!
    alarm_start();
    mailLastSeen = lastSeen;
  }
}

// Draw the display for the specified time and alarm time.
void draw_display(int time, int alarmTime) {
  Tft.fillRectangle(0, 0, 240, 320, BLACK);
  char timeString[9];
  // Draw time text.
  time_to_string(time, timeString);
  Tft.drawString(timeString, 50, 290, 3, CLOCK_COLOR);
  // Draw clock.
  Tft.drawCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS, CLOCK_COLOR);
  // Draw hour ticks.
  for (int i = 0; i < 12; ++i) {
    draw_clock_angle((i / 12.0) * TWO_PI, CLOCK_RADIUS - CLOCK_TICK_SIZE, CLOCK_RADIUS);
  }
  // Draw hour hand.
  int hour12, minute;
  bool am;
  time_to_hour_minute(time, hour12, minute, am);
  draw_clock_angle(((hour12 - 3) / 12.0) * TWO_PI + ((minute / 60.0) * (TWO_PI / 12.0)), 0, 50);
  // Draw minute hand.
  draw_clock_angle(((minute - 15) / 60.0) * TWO_PI, 0, 70);
  // Draw alarm time.
  if (alarmTime >= 0) {
    time_to_string(alarmTime, timeString);
    Tft.drawString("ALARM:", 40, 200, 2, CLOCK_COLOR);
    Tft.drawString(timeString, 120, 200, 2, CLOCK_COLOR);
    // Draw alarm cancel button.
    Tft.drawString("CANCEL", CANCEL_X + 30, CANCEL_Y + 5, 2, CLOCK_COLOR);
    Tft.drawRectangle(CANCEL_X, CANCEL_Y, BUTTON_HEIGHT, BUTTON_WIDTH, CLOCK_COLOR);
  }
  // Draw alarm refresh button.

  Tft.drawString("REFRESH", 20, 250, 2, CLOCK_COLOR);
  Tft.drawRectangle(20, 250, 85, 25, CLOCK_COLOR);

  //Draw test button
  Tft.drawString("TEST", 150, 250, 2, CLOCK_COLOR);
  Tft.drawRectangle(150, 250, 50, 25, CLOCK_COLOR);

  //draw set button
  Tft.drawString("SET", 100, 200, 2, CLOCK_COLOR);
  Tft.drawRectangle(100, 200, 40, 20, CLOCK_COLOR);

}

void draw_set_display()
{
  Tft.fillRectangle(0, 0, 240, 320, BLACK);
  //hour buttons
  Tft.drawRectangle(25, 20, 50, 50, CLOCK_COLOR);
  Tft.drawString("+", 25, 20, 7, CLOCK_COLOR);
  Tft.drawRectangle(25, 125, 50, 50, CLOCK_COLOR);
  Tft.drawString("-", 25, 125, 7, CLOCK_COLOR);

  //min buttons
  Tft.drawRectangle(125, 20, 50, 50, CLOCK_COLOR);
  Tft.drawString("+", 125, 20, 7, CLOCK_COLOR);
  Tft.drawRectangle(125, 125, 50, 50, CLOCK_COLOR);
  Tft.drawString("-", 125, 125, 7, CLOCK_COLOR);

  char hourArr[3];
  char minuteArr[3];

  String hourString;
  String minuteString;

  hourString = (String) setHour;
  minuteString = (String) setMin;

  hourString.toCharArray(hourArr, 3);
  minuteString.toCharArray(minuteArr, 30);

  Tft.drawString(hourArr, 30, 85, 3, CLOCK_COLOR);
  Tft.drawString(minuteArr, 130, 85, 3, CLOCK_COLOR);

  //ok button
  Tft.drawRectangle(185, 85, 40, 25, CLOCK_COLOR);
  Tft.drawString("OK", 185, 85, 3, CLOCK_COLOR);
}

// Draw segment of a line from the center of the clock outward at the specified angle and start/end radius.
void draw_clock_angle(float angle, float startRadius, float endRadius) {
  int startX = int(startRadius * cos(angle)) + CLOCK_CENTER_X;
  int startY = int(startRadius * sin(angle)) + CLOCK_CENTER_Y;
  int endX = int(endRadius * cos(angle)) + CLOCK_CENTER_X;
  int endY = int(endRadius * sin(angle)) + CLOCK_CENTER_Y;
  Tft.drawLine(startX, startY, endX, endY, CLOCK_COLOR);
}

