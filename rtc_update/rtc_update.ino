#include <Wire.h>
#include "RTClib.h"   // Jeelab's fantastic RTC library. 
RTC_DS3231 rtc;
int update_mode = 2;
volatile bool isUpdate = false;


void setup ()
{
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  DateTime now = rtc.now();
  DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
  if (now.unixtime() < compiled.unixtime()) {
    Serial.println("RTC is older than compile time! Updating");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop ()
{
  showTime();

  delay(1000);

}

void showTime()
{
  DateTime nowDT = rtc.now();
  Serial.print(nowDT.hour(), DEC); Serial.print(':');

  byte myMin = nowDT.minute();
  if (myMin < 10)
  {
    Serial.print('0');
  }
  Serial.print(nowDT.minute(), DEC); Serial.print(':');

  byte mySec = nowDT.second();
  if (mySec < 10)
  {
    Serial.print('0');
  }
  Serial.println(nowDT.second(), DEC);
}
