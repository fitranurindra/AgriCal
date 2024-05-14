/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-microsd-card-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#include <string.h>

//Libraries for DHT
#include "DHT.h"

// Libraries to get time from NTP Server
#include <WiFi.h>
#include "time.h"

#define DHTTYPE DHT11
#define DHTPIN 17

#define BUTTON_PIN 16

#define MAX_LEN 255
#define MAX_DATA 1440

DHT dht(DHTPIN, DHTTYPE);

// Replace with your network credentials
const char* ssid     = "SANGKURIANG L-1";
const char* password = "SANGK11*";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 15000;

// Variables to hold sensor readings
float h;
float t;
float f;
float hic;
float hif;

String data_store[MAX_DATA];
String temp[6];

int StringCount = 0;

int countSaveData = 0;

String dataMessage;

uint32_t totalLineData = 0;

// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

// Variable to save current epoch time
unsigned long epochTime; 

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// Initialize SD card
void initSDCard(){
   if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// Write to the SD card
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

int countLine(fs::FS &fs, const char * path){
  File file = fs.open(path);
  uint32_t lineStart = 0;
  if (file) {
    while (file.available()) {
      lineStart = file.position();
      if (!file.find((char*) "\n"))
        break;
    }
    file.close();
  } 
  return lineStart;
}

void readLatestLine(fs::FS &fs, const char * path, uint32_t position){
  Serial.printf("Reading latest line on file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  
  file.seek(position);
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

// Append data to the SD card
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void setup() {
  Serial.begin(115200);
  
  initWiFi();
  dht.begin();
  initSDCard();
  configTime(0, 0, ntpServer);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Epoch Time, Humidity (%), Temperature (°C), Temperature (°F), Heat Index (°C), Heat Index (°F) \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);
  Serial.println(buttonState);

  if (buttonState == HIGH){
    if (countSaveData != 0){
      Serial.println("Send Stored Data to AWS IOT Cloud");
      Serial.println("Sent data: ");
      for (int i = 0; i < countSaveData; i++){
        // publish sebanyak data yang disave
        splitString(data_store[i], temp);
        Serial.printf("%s : %s : ", temp[0], temp[5]);
        Serial.println(StringCount);
        // Serial.println(data_store[i]);
      }

      splitString(data_store[1], temp);
      float a = temp[5].toFloat();
      Serial.println(a);

      Serial.println(data_store[0]);

      memset(data_store, 0, sizeof(data_store));
      countSaveData = 0;
    }
  }

  if ((millis() - lastTime) > timerDelay) {
    //Get epoch time
    epochTime = getTime();

    dhtUpdate();

    //Concatenate all info separated by commas
    dataMessage = String(epochTime) + "," + String(h) + "," + String(t) + "," + String(f) + "," + String(hic) + "," + String(hif) + "\r\n";
    Serial.print("Saving data to SD Card: ");
    Serial.println(dataMessage);

    //Append the data to file
    appendFile(SD, "/data.txt", dataMessage.c_str());

    Serial.println("Counting line on file: /data.txt\n");
    totalLineData = countLine(SD, "/data.txt");
    if (totalLineData == 0){
      Serial.println("Failed to open file for counting the line");
    }
    else{
      Serial.print("Total line on file: ");
      Serial.println(totalLineData);
    }

    if (buttonState == LOW){
      Serial.println("Interlock! Save Data\n");
      saveData(SD, "/data.txt", totalLineData);
      Serial.printf("Total Stored Data: %d\n", countSaveData);
    }
    else{
      Serial.println("Send Data to AWS IOT Cloud");
    }

    // readFile(SD, "/data.txt");

    lastTime = millis();
  }

  delay(1000);
}

void splitString(String str, String* strs){
  // Split the string into substrings
  StringCount = 0;
  while (str.length() > 0){
    int index = str.indexOf(',');
    if (index == -1){ // No comma found
      strs[StringCount++] = str;
      break;
    }
    else{
      strs[StringCount++] = str.substring(0, index);
      str = str.substring(index+1);
    }
  }
}

void saveData(fs::FS &fs, const char * path, uint32_t position){
  File file = SD.open(path);

  if (file) {
    file.seek(position);
    while (file.available()) {
      String buffer = file.readStringUntil('\n');
      data_store[countSaveData] = buffer;
      countSaveData++;
    }
    file.close();
  }
}

void dhtUpdate(){
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  hic = dht.computeHeatIndex(t, h, false);
}