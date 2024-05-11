#include <Wire.h>
#include "RTClib.h"   // Jeelab's fantastic RTC library. 
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int mode = 2;
volatile bool isUpdate = false;


void setup ()
{
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //rtc.adjust(DateTime(2021, 12, 31, 11, 59, 57));//set date-time manualy:yr,mo,dy,hr,mn,sec
}

void loop ()
{
  int amtUpdate = xUpdate(mode);
  int* time_update = (int*) malloc(amtUpdate * sizeof(int)); 
  updateTime(time_update, mode);  

  DateTime now = rtc.now();

  int i;

  for (i = 0; i < amtUpdate; i++){
    if(now.minute() == time_update[i]){
      isUpdate = true;
      break;
    }
    else{
      isUpdate = false;
    }
  }

  if (isUpdate){
    showDate();     //show date on Serial Monitor
    showTime();    //Current time: 24 Hrs
    showTemp();   //show temperature
    delay(10000);
    // timeDelay();  //2 minute time delay as counted by RTC
  }
  else{
    int wait;
    for(int i = 1; i < amtUpdate; i++){
      if (time_update[i] == 0){
        time_update[i] = 60;
      }

      if (now.minute() - time_update[i] < 0){
        wait = abs(now.minute() - time_update[i]);
        break;
      }
    }
    Serial.print("Wait ");
    Serial.print(wait);
    Serial.println(" Minutes Again");

    delay(1000);
  }

}

int xUpdate(int mode){
  int amtUpdate;
  if (mode == 0){
    amtUpdate = 2;
  }
  else if (mode == 1){
    amtUpdate = 4;
  } 
  else{
    amtUpdate = 30;
  }
  return amtUpdate;
}

void showDate()
{
  DateTime nowDT = rtc.now();
  Serial.print(daysOfTheWeek[nowDT.dayOfTheWeek()]);
  Serial.print(" ");
  Serial.print(nowDT.day(), DEC); Serial.print('/');
  Serial.print(nowDT.month(), DEC); Serial.print('/');
  Serial.print(nowDT.year(), DEC); Serial.print(" ==> ");
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

void updateTime(int* time_update, int mode){  
  if (mode == 0){ // Update Data Setiap 30 Menit
    time_update[0] = 0;
    time_update[1] = 30;
  }
  else if (mode == 1){ // Update Data Setiap 15 Menit
    time_update[0] = 0;
    time_update[1] = 15;
    time_update[2] = 30;
    time_update[3] = 45;
  }
  else{ // Update Data Setiap 10 Menit
    // time_update[0] = 0;
    // time_update[1] = 10;
    // time_update[2] = 20;
    // time_update[3] = 30;
    // time_update[4] = 40;
    // time_update[5] = 50;
    time_update[0] = 0;
    time_update[1] = 2;
    time_update[2] = 4;
    time_update[3] = 6;
    time_update[4] = 8;
    time_update[5] = 10;
    time_update[6] = 12;
    time_update[7] = 14;
    time_update[8] = 16;
    time_update[9] = 18;
    time_update[10] = 20;
    time_update[11] = 22;
    time_update[12] = 24;
    time_update[13] = 26;
    time_update[14] = 28;
    time_update[15] = 30;
    time_update[16] = 32;
    time_update[17] = 34;
    time_update[18] = 36;
    time_update[19] = 38;
    time_update[20] = 40;
    time_update[21] = 42;
    time_update[22] = 44;
    time_update[23] = 46;
    time_update[24] = 48;
    time_update[25] = 50;
    time_update[26] = 52;
    time_update[27] = 54;
    time_update[28] = 56;
    time_update[29] = 58;
  }
}

void showTemp()
{
  float myTemp = rtc.getTemperature();
  Serial.print("  Temperature: ");
  Serial.print(myTemp, 2);
  Serial.println(" degC");
}

// byte bcdMinute()
// {
//   DateTime nowTime = rtc.now();
//   if (nowTime.minute() == 0 )
//   {
//     prMin = 0;
//     return prMin;
//   }
//   else
//   {
//     DateTime nowTime = rtc.now();
//     return nowTime.minute();
//   }
// }