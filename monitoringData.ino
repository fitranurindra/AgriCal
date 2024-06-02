#include "secrets.h"

#include <TFT_eSPI.h> // Hardware-specific library
#include "Free_Fonts.h"

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"

#include <ADSWeather.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BH1750.h>

#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#include "RTClib.h"

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#include <string.h>

#define FSB12 &FreeSerifBold12pt7b

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// #define SEALEVELPRESSURE_HPA (1013.25)

#define ANEMOMETER_PIN 27
#define VANE_PIN A0
#define RAIN_PIN 12

#define soilMoisturePin A3

#define CALC_INTERVAL 1000

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define voltagePin A6

#define MAX_DATA 1440

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

ADSWeather ws1(RAIN_PIN, VANE_PIN, ANEMOMETER_PIN); //This should configure all pins correctly
Adafruit_BME280 bme;
BH1750 lightMeter;

SoftwareSerial nodemcu(9,10);

RTC_DS3231 rtc;

TFT_eSPI tft = TFT_eSPI(); 

float rainAmmount;
float windSpeed;
int windDirection;
float windGust;

int soilMoistureValue = 0;
const int AirValue = 500;
const int WaterValue = 245;
float moisturePercentage;

float tempBME;
float humBME;
float pressBME;

float lux;
float solarRadiation;

float analogValue = 0;
float voltage = 0;

String date;
String timeRTC;

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int update_mode = 2;
volatile bool isUpdate = false;

String data_store[MAX_DATA];
String temp[14];

int StringCount = 0;
int countSaveData = 0;

String dataMessage;
String lastData;
uint32_t totalLineData = 0;

unsigned long previousMillis = 0;
unsigned long interval = 30000;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  nodemcu.begin(115200);

  initSDCard();

  File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "date, time, voltage (V), lux (lx), solarRadiation (W/m^2), moisture (%), humidity (%), temperature (°C), pressure (mbar), windSpeed (km/h), windDirection (°), windGust (km/h), rainAmmount (l) \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();

  tft.begin();
  tft.setRotation(1);

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  rtc.begin();

  DateTime now = rtc.now();
  DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
  if (now.unixtime() < compiled.unixtime()) {
    Serial.println("RTC is older than compile time! Updating");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lightMeter.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  if (!bme.begin(0x76)) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		while (1);
  }

  // delete old config
  WiFi.disconnect(true);

  delay(1000);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connectAWS();

  tftStartDisplay();
  display.clearDisplay();
  display.setTextSize(2.5);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  // Display static text
  display.println("Agrical");
  display.display();
  delay(5000);

  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), ws1.countRain, FALLING); //ws1.countRain is the ISR for the rain gauge.
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), ws1.countAnemometer, FALLING); //ws1.countAnemometer is the ISR for the anemometer.

  Serial.println();
  Serial.println("Program started");
}

void loop() {
  int amtUpdate = xUpdate(update_mode);
  int* time_update = (int*) malloc(amtUpdate * sizeof(int)); 
  updateTime(time_update, update_mode); 

  DateTime now = rtc.now();

  for (int i = 0; i < amtUpdate; i++){
    if(now.minute() == time_update[i] && now.second() < 13){
      isUpdate = true;
      break;
    }
    else{
      isUpdate = false;
    }
  }

  if (WiFi.status() == WL_CONNECTED){
    if(!client.connect(THINGNAME)){
      connectAWS();
    }

    File file = SD.open("/data_backup.txt");
    if (file){
      Serial.println("Send Stored Data to AWS IOT Cloud");

      totalLineData = countLine(SD, "/data_backtup.txt");
      lastData = LatestData(SD, "/data_backup.txt", totalLineData);

      splitString(lastData, temp);
      countSaveData = temp[0].toInt();
      saveAllData(SD, "/data_backup.txt", data_store);

      Serial.printf("Send %d data: \n", countSaveData);

      for (int i = 0; i < countSaveData; i++){
        // publish sebanyak data yang disave
        splitString(data_store[i], temp);
        Serial.printf("%s : %s : %s : %s : %s : %s : %s : %s : %s : %s : %s : %s : %s : %s", temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6], temp[7], temp[8], temp[9], temp[10], temp[11], temp[12], temp[13]);
        Serial.println(StringCount);
        date = temp[1];
        timeRTC = temp[2];
        voltage = temp[3].toFloat();
        lux = temp[4].toFloat();
        solarRadiation = temp[5].toFloat();
        moisturePercentage = temp[6].toFloat();
        humBME = temp[7].toFloat();
        tempBME = temp[8].toFloat();
        pressBME = temp[9].toFloat();
        windSpeed = temp[10].toFloat();
        windDirection = temp[11].toInt();
        windGust = temp[12].toFloat();
        rainAmmount = temp[13].toFloat();

        publishMessage();
        delay(1000);
        // Serial.println(data_store[i]);
      }
      // splitString(data_store[1], temp);
      // float a = temp[5].toFloat();
      // Serial.println(a);
      // Serial.println(data_store[0]);

      memset(data_store, 0, sizeof(data_store));
      deleteFile(SD, "/data_backup.txt");
    }
    file.close();
  }

  if (isUpdate){
    showDate();     //show date on Serial Monitor
    showTime();    //Current time: 24 Hrs

    // Update Weather Data
    bme_update();
    ws1.update();
    weather_update();
    voltage_update();
    soilMoisture_update();
    lux_update();

    serialMonitor();

    dataMessage = date + "," + timeRTC + "," + String(voltage) + "," + String(lux) + "," + String(solarRadiation) + "," + String(moisturePercentage) + "," + String(humBME) + "," + String(tempBME) + "," + String(pressBME) + "," + String(windSpeed) + "," + String(windDirection) + "," + String(windGust) + "," + String(rainAmmount) + "\r\n";
    Serial.printf("\nSaving data to SD Card: ");
    Serial.print(dataMessage);

    //Append the data to file
    appendFile(SD, "/data.txt", dataMessage.c_str());

    tftDisplayData();
    oledDisplay();

    if (WiFi.status() != WL_CONNECTED){
      Serial.println("Interlock! Save Data to SD Card\n");
      File file = SD.open("/data_backup.txt");
      if(!file) {
        Serial.println("File for backup data doesn't exist");
        Serial.println("Creating file for backup...");
        writeFile(SD, "/data_backup.txt", "");
        countSaveData = 0;
      }
      else {
        Serial.println("File for backup data already exists");  
        totalLineData = countLine(SD, "/data_backup.txt");
        lastData = LatestData(SD, "/data_backup.txt", totalLineData);
        splitString(lastData, temp);
        countSaveData = temp[0].toInt();
      }
      file.close();
      countSaveData += 1;
      String temp_dataMessage = String(countSaveData) + "," + dataMessage;

      //Append the data to backup file
      appendFile(SD, "/data_backup.txt", temp_dataMessage.c_str());
      Serial.printf("Total Stored Data: %d\n", countSaveData);
    }
    else{
      publishMessage();
      Serial.println("Send Data to AWS IOT Cloud");
    }

  }
  else if(WiFi.status() != WL_CONNECTED){
    int wait = waitTime(amtUpdate, time_update, now.minute());
    Serial.print("Wait ");
    Serial.print(wait);
    Serial.println(" Minutes Again");
    showTime();

    tftReconnectDisplay();
    tft.setFreeFont(FSB12);   
    tft.println();         
    tft.print("Next Update Data in ");
    tft.print(wait);
    tft.print("Minutes");

    display.clearDisplay();
    display.display();
  }
  else{
    tft.fillScreen(TFT_BLACK);
    display.clearDisplay();
    display.display();

    int wait = waitTime(amtUpdate, time_update, now.minute());

    Serial.print("Wait ");
    Serial.print(wait);
    Serial.println(" Minutes Again");
    showTime();
  }

  client.loop();

  delay(10000);
}

String LatestData(fs::FS &fs, const char * path, uint32_t position){
  File file = SD.open(path);
  String buffer;

  if (file) {
    file.seek(position);
    while (file.available()) {
      buffer = file.readStringUntil('\n');
    }
    file.close();
  }

  return buffer;
}

// Print the header for a display screen
void header(const char *string){
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA, TFT_BLUE);
  tft.fillRect(0, 0, 480, 30, TFT_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(string, 239, 2, 4); // Font 4 for fast drawing with background
}

void saveAllData(fs::FS &fs, const char * path, String* store){
  File file = SD.open(path);
  String buffer;
  int i = 0;

  if (file) {
    while (file.available()) {
      buffer = file.readStringUntil('\n');
      store[i++] = buffer;
    }
    file.close();
  }
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

// Initialize SD card
void initSDCard(){
  delay(500);
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
  } 
  else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } 
  else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } 
  else {
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
  } 
  else {
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
  } 
  else {
    Serial.println("Append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } 
  else {
    Serial.println("Delete failed");
  }
}

int waitTime(int a, int* b, int c){
  int wait = 0;
  for(int i = 0; i < a; i++){
    if (c >= b[a-1]){
      b[i] = 60;
    }

    if (c - b[i] < 0){
      wait = abs(c - b[i]);
      break;
    }
  }
  return wait;
}

String formatDate(int a, int b, int c) { 
  String formattedDate;
  String temp_a = String(a);
  String temp_b = String(b);
  String temp_c = String(c);

  if (a < 10){
    temp_a = "0" + temp_a;
  }
  if (b < 10){
    temp_b = "0" + temp_b;
  }
  if (c < 10){
    temp_c = "0" + temp_c;
  }

  formattedDate = temp_c + "-" + temp_b + "-" + temp_a;

  return formattedDate; 
}

String formatTime(int a, int b, int c) { 
  String formattedTime;
  String temp_a = String(a);
  String temp_b = String(b);
  String temp_c = String(c);

  if (a < 10){
    temp_a = "0" + temp_a;
  }
  if (b < 10){
    temp_b = "0" + temp_b;
  }
  if (c < 10){
    temp_c = "0" + temp_c;
  }

  formattedTime = temp_a + ":" + temp_b + ":" + temp_c;

  return formattedTime; 
}

void oledDisplay(){
  display.clearDisplay();

  display.setTextSize(0.7);

  display.setCursor(0, 0);
  display.print("Light   : ");
  display.print(lux);
  display.println(" lx");

  display.setCursor(0, 8);
  display.print("Irr     : ");
  display.print(solarRadiation);
  display.println(" W/m^2");

  display.setCursor(0, 16);
  display.print("Moisture: ");
  display.print(moisturePercentage);
  display.println(" %");

  display.setCursor(0, 24);
  display.print("Humidity: ");
  display.print(humBME);
  display.println(" %");

  display.setCursor(0, 32);
  display.print("Temp    : ");
  display.print(tempBME);
  display.println(" °C");

  display.setCursor(0, 40);
  display.print("Pressure: ");
  display.print(pressBME);
  display.println("mbar");

  display.setCursor(0, 48);
  display.print("Wind Spd: ");
  display.print(windSpeed);
  display.println(" km/h");

  display.setCursor(0, 56);
  display.print("Voltage : ");
  display.print(voltage);
  display.println(" V");

  // display.print("Wind Dir: ");
  // display.print(windDirection);
  // display.println(" *");

  display.display(); 
}

void serialMonitor(){
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");

  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  Serial.print("Solar Radiation: ");
  Serial.print(solarRadiation);
  Serial.println(" W/m2");

  Serial.print("Moisture: ");
  Serial.print(moisturePercentage);
  Serial.println("%");

  Serial.print("Humidity: ");
  Serial.print(humBME);
  Serial.println("%");

  Serial.print("Temperature: ");
  Serial.print(tempBME);
  Serial.println("°C");

  Serial.print("Pressure: ");
  Serial.print(pressBME);
  Serial.println("mbar");

  Serial.print("Wind speed: ");
  Serial.print(windSpeed);
  Serial.println(" km/h");

  Serial.print("Gusting at: ");
  Serial.print(windGust);
  Serial.println(" km/h");

  Serial.print("Wind Direction: ");
  Serial.print(windDirection);
  Serial.println(" °");

  Serial.print("Total Rain: ");
  Serial.print(rainAmmount);
  Serial.println(" liter");

  Serial.println("-------------------------------");
}

void tftDisplayData(){
  int xpos =  0;
  int ypos = 40;

  tft.fillScreen(TFT_NAVY); // Clear screen to navy background

  header("AgriCal");

  // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
  // even if we specify it.
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
  // tft.setFreeFont(&FreeSerif9pt7b);
  tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
  // as the datum, so we must move the cursor down a line from the 0,0 position
  tft.print("humidity (%): ");  // Print the font name onto the TFT screen
  tft.print(humBME);

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Temperature (°C): ");  // Print the font name onto the TFT screen
  tft.print(tempBME);

  tft.setFreeFont(FSB18);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Pressure (mbar): ");  // Print the font name onto the TFT screen
  tft.print(pressBME);

  tft.setFreeFont(FSB24);       // Select Free Serif 24 point font
  tft.println();                // Move cursor down a line
  tft.print("Lux (lx): ");  // Print the font name onto the TFT screen
  tft.print(lux);
}

void tftReconnectDisplay(){
    int xpos =  0;
    int ypos = 40;

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
    // Select different fonts to draw on screen using the print class
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    tft.fillScreen(TFT_NAVY); // Clear screen to navy background

    header("WiFi not connected");

    // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
    // even if we specify it.
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

    tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
    // tft.setFreeFont(&FreeSerif9pt7b);
    tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
    // as the datum, so we must move the cursor down a line from the 0,0 position
    tft.print("Reconnecting...");  // Print the font name onto the TFT screen
}

void tftStartDisplay(){
  int xpos =  50;
  int ypos = 50;

  tft.fillScreen(TFT_NAVY); // Clear screen to navy background

  header("Sistem Kalender Pertanian");

  // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
  // even if we specify it.
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
  // tft.setFreeFont(&FreeSerif9pt7b);
  tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
  // as the datum, so we must move the cursor down a line from the 0,0 position
  tft.print("AgriCal");  // Print the font name onto the TFT screen
}

void connectAWS(){
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  // Serial.println("Connecting to Wi-Fi");
 
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.printf("\nConnecting to AWS IOT\n");
 
  unsigned long currentMillis = millis();

  // if ((!client.connect(THINGNAME)) && (currentMillis - previousMillis >= interval)) {
  //   Serial.print(millis());
  //   Serial.println("Reconnecting to AWS IOT...");
  //   previousMillis = currentMillis;
  // }
  if (WiFi.status() == WL_CONNECTED){
    while (!client.connect(THINGNAME)){
      Serial.print(".");
      delay(100);
    }
  }
 
  if (!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.printf("\nAWS IoT Connected!\n");
}

void publishMessage(){
  StaticJsonDocument<200> doc1;
  StaticJsonDocument<200> doc2;
  //Assign collected data to JSON Object
  // DateTime nowDT = rtc.now();
  // date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
  // timeRTC = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
  doc1["date"] = date;
  doc1["time"] = timeRTC;
  doc1["voltage"] = voltage;
  doc1["lux"] = lux;
  doc1["solarRadiation"] = solarRadiation;
  doc1["moisture"] = moisturePercentage;
  doc1["humidity"] = humBME;

  doc2["date"] = date;
  doc2["time"] = timeRTC;
  doc2["temperature"] = tempBME;
  doc2["pressure"] = pressBME;
  doc2["windSpeed"] = windSpeed;
  doc2["windDirection"] = windDirection;
  doc2["windGust"] = windGust;
  doc2["rainAmount"] = rainAmmount;

  char jsonBuffer1[512];
  char jsonBuffer2[512];
  serializeJson(doc1, jsonBuffer1); // print to client
  serializeJson(doc2, jsonBuffer2); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer1);
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer2);
}

// void publishMessage(){
//   StaticJsonDocument<200> doc;
//   //Assign collected data to JSON Object
//   DateTime nowDT = rtc.now();
//   date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
//   timeRTC = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
//   String timestamp = date + "," + timeRTC;
//   doc["timestamp"] = timestamp;
//   doc["voltage"] = voltage;
//   doc["moisture"] = moisturePercentage;

//   char jsonBuffer[512];
//   serializeJson(doc, jsonBuffer); // print to client
 
//   client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
// }

// void publishMessage(){
//   StaticJsonDocument<200> doc;
//   //Assign collected data to JSON Object
//   DateTime nowDT = rtc.now();
//   date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
//   timeRTC = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
//   doc["date"] = date;
//   doc["time"] = timeRTC;
  
//   doc["voltage"] = voltage;

//   doc["lux"] = lux;
//   doc["solarRadiation"] = solarRadiation;

//   doc["moisture"] = moisturePercentage;

//   doc["humidity"] = humBME;
//   doc["temperature"] = tempBME;
//   doc["pressure"] = pressBME;

//   doc["windSpeed"] = windSpeed;
//   doc["windDirection"] = windDirection;
//   doc["windGust"] = windGust;
//   doc["rainAmmount"] = rainAmmount;
//   char jsonBuffer[512];
//   serializeJson(doc, jsonBuffer); // print to client
 
//   client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
// }

void messageHandler(char* topic, byte* payload, unsigned int length){
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void bme_update() {
  humBME = bme.readHumidity();
  tempBME = bme.readTemperature();
  pressBME = bme.readPressure() / 100.0F;
}

void weather_update(){
  rainAmmount = ws1.getRain() / 4000;
  windSpeed = ws1.getWindSpeed() / 10;
  windDirection = ws1.getWindDirection();
  windGust = ws1.getWindGust() / 10;
}

void voltage_update(){
  analogValue = analogRead(voltagePin);
  voltage = map(analogValue, 0, 4095, 0.0, 3280.0); //deliberate mistake ;)
  voltage /= 200;
  if (voltage < 11.9){
    voltage *= 1.1;
  }
  else if ((voltage > 11.9) && (voltage < 12.9)){
    voltage *= 1.08;
  }
  else if ((voltage > 12.9) && (voltage < 13.9)){
    voltage *= 1.04;
  }
  else if ((voltage > 13.9) && (voltage < 14.6)){
    voltage *= 1.02;
  }
  else if (voltage > 14.61){
    voltage *= 0.98;
  }
}

void soilMoisture_update(){
  // soilMoistureValue = analogRead(soilMoisturePin);
  soilMoistureValue = analogRead(soilMoisturePin)*470/2480;
  moisturePercentage = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
}

void lux_update(){
  lux = lightMeter.readLightLevel();
  solarRadiation = (lux*0.0079);
}

int xUpdate(int update_mode){
  int amtUpdate;
  if (update_mode == 0){
    amtUpdate = 2;
  }
  else if (update_mode == 1){
    amtUpdate = 4;
  } 
  else{
    amtUpdate = 30;
  }
  return amtUpdate;
}

void showDate(){
  DateTime nowDT = rtc.now();
  Serial.print(daysOfTheWeek[nowDT.dayOfTheWeek()]);
  Serial.print(" ");
  date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
  Serial.print(date); Serial.print(" ==> ");
}

void showTime(){
  DateTime nowDT = rtc.now();
  timeRTC = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
  Serial.println(timeRTC);
}

void updateTime(int* time_update, int update_mode){
  int totalUpdate;

  if (update_mode == 0){ // Update setiap 30 menit
    totalUpdate = 2;
  }
  else if(update_mode == 1){ // Update setiap 15 menit
    totalUpdate = 4;
  }
  else{ // Update setiap 2 menit
    totalUpdate = 30;
  } 

  for(int i = 0; i < totalUpdate; i++){
    time_update[i] = i*60/totalUpdate;
  }

  // if (update_mode == 0){ // Update Data Setiap 30 Menit
  //   time_update[0] = 0;
  //   time_update[1] = 30;
  // }
  // else if (update_mode == 1){ // Update Data Setiap 15 Menit
  //   time_update[0] = 0;
  //   time_update[1] = 15;
  //   time_update[2] = 30;
  //   time_update[3] = 45;
  // }
  // else{ // Update Data Setiap 10 Menit
  //   // time_update[0] = 0;
  //   // time_update[1] = 10;
  //   // time_update[2] = 20;
  //   // time_update[3] = 30;
  //   // time_update[4] = 40;
  //   // time_update[5] = 50;
  //   for(int i = 0; i < 30; i++){

  //   }
  //   time_update[0] = 0;
  //   time_update[1] = 2;
  //   time_update[2] = 4;
  //   time_update[3] = 6;
  //   time_update[4] = 8;
  //   time_update[5] = 10;
  //   time_update[6] = 12;
  //   time_update[7] = 14;
  //   time_update[8] = 16;
  //   time_update[9] = 18;
  //   time_update[10] = 20;
  //   time_update[11] = 22;
  //   time_update[12] = 24;
  //   time_update[13] = 26;
  //   time_update[14] = 28;
  //   time_update[15] = 30;
  //   time_update[16] = 32;
  //   time_update[17] = 34;
  //   time_update[18] = 36;
  //   time_update[19] = 38;
  //   time_update[20] = 40;
  //   time_update[21] = 42;
  //   time_update[22] = 44;
  //   time_update[23] = 46;
  //   time_update[24] = 48;
  //   time_update[25] = 50;
  //   time_update[26] = 52;
  //   time_update[27] = 54;
  //   time_update[28] = 56;
  //   time_update[29] = 58;
  // }
}

#ifndef LOAD_GLCD
//ERROR_Please_enable_LOAD_GLCD_in_User_Setup
#endif

#ifndef LOAD_FONT2
//ERROR_Please_enable_LOAD_FONT2_in_User_Setup!
#endif

#ifndef LOAD_FONT4
//ERROR_Please_enable_LOAD_FONT4_in_User_Setup!
#endif

#ifndef LOAD_FONT6
//ERROR_Please_enable_LOAD_FONT6_in_User_Setup!
#endif

#ifndef LOAD_FONT7
//ERROR_Please_enable_LOAD_FONT7_in_User_Setup!
#endif

#ifndef LOAD_FONT8
//ERROR_Please_enable_LOAD_FONT8_in_User_Setup!
#endif

#ifndef LOAD_GFXFF
ERROR_Please_enable_LOAD_GFXFF_in_User_Setup!
#endif

