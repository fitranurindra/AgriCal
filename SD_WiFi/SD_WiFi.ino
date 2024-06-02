#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"

#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#include <string.h>

//Libraries for DHT
#include "DHT.h"


// Libraries to get time from NTP Server
#include "time.h"

#include <TFT_eSPI.h> // Hardware-specific library

#include "Free_Fonts.h" // Include the header file attached to this sketch

TFT_eSPI tft = TFT_eSPI();                   // Invoke custom library with default width and height

#define DHTTYPE DHT11
#define DHTPIN 17

#define BUTTON_PIN 16

#define MAX_LEN 255
#define MAX_DATA 1440

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

DHT dht(DHTPIN, DHTTYPE);

// // Replace with your network credentials
// const char* ssid     = "unga";
// const char* password = "punyabunga";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

// Variables to hold sensor readings
float h;
float t;
float f;
float hic;
float hif;

String data_store[MAX_DATA];
String temp[7];

int StringCount = 0;

int countSaveData = 0;

String dataMessage;

String lastData;

uint32_t totalLineData = 0;

// NTP server to request epoch time
// const char* ntpServer = "pool.ntp.org";

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

// // Initialize WiFi
// void initWiFi() {
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);
//   Serial.print("Connecting to WiFi ..");
//   while (WiFi.status() != WL_CONNECTED) { // while loop true selama tidak terhubung
//     Serial.print('.');
//     delay(1000);
//   }
//   Serial.println(WiFi.localIP());
// }

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

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void tftDisplay(){
  int xpos =  0;
  int ypos = 40;

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Select different fonts to draw on screen using the print class
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.fillScreen(TFT_NAVY); // Clear screen to navy background

  header("Draw free fonts using print class");

  // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
  // even if we specify it.
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
  // tft.setFreeFont(&FreeSerif9pt7b);
  tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
  // as the datum, so we must move the cursor down a line from the 0,0 position
  tft.print("humidity: ");  // Print the font name onto the TFT screen
  tft.print(h);

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Temperature (*C): ");  // Print the font name onto the TFT screen
  tft.print(t);

  tft.setFreeFont(FSB18);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Temperature (*F): ");  // Print the font name onto the TFT screen
  tft.print(f);

  tft.setFreeFont(FSB24);       // Select Free Serif 24 point font
  tft.println();                // Move cursor down a line
  tft.print("hic: ");  // Print the font name onto the TFT screen
  tft.print(hic);

  delay(10000);
}

void publishMessage(){
  StaticJsonDocument<200> doc;
  //Assign collected data to JSON Object

  doc["humidity"] = h;
  doc["temperatureC"] = t;
  doc["temperatureF"] = f;
  doc["hiC"] = hic;

  char jsonBuffer[512];

  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(char* topic, byte* payload, unsigned int length){
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // connectAWS();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.printf("\nConnecting to AWS IOT\n");
 
  while (!client.connect(THINGNAME)){
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.printf("\nAWS IoT Connected!\n");
}

void setup() {
  Serial.begin(115200);
  
  // initWiFi();

  dht.begin();
  initSDCard();

  tft.begin();

  tft.setRotation(1);

  // configTime(0, 0, ntpServer);
  
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

  // delete old config
  WiFi.disconnect(true);

  delay(1000);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connectAWS();
    
  Serial.println();

  Serial.println("Program Started");
}

void loop() {
  // int buttonState = digitalRead(BUTTON_PIN);
  // Serial.println(buttonState);

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
        Serial.printf("%s : %s : ", temp[0], temp[5]);
        Serial.println(StringCount);
        h = temp[2].toFloat();
        t = temp[3].toFloat();
        f = temp[4].toFloat();
        hic = temp[5].toFloat();

        publishMessage();
        delay(1000);
        // Serial.println(data_store[i]);
      }

      splitString(data_store[1], temp);
      float a = temp[5].toFloat();
      Serial.println(a);

      Serial.println(data_store[0]);

      memset(data_store, 0, sizeof(data_store));

      deleteFile(SD, "/data_backup.txt");
    }
    file.close();
  }

  if ((millis() - lastTime) > timerDelay) {
    //Get epoch time
    epochTime = getTime();

    dhtUpdate();
    publishMessage();

    //Concatenate all info separated by commas
    dataMessage = String(epochTime) + "," + String(h) + "," + String(t) + "," + String(f) + "," + String(hic) + "," + String(hif) + "\r\n";
    Serial.printf("\nSaving data to SD Card: ");
    Serial.print(dataMessage);

    //Append the data to file
    appendFile(SD, "/data.txt", dataMessage.c_str());

    tftDisplay();

    // Serial.println("Counting line on file: /data.txt\n");
    // totalLineData = countLine(SD, "/data.txt");
    // if (totalLineData == 0){
    //   Serial.println("Failed to open file for counting the line");
    // }
    // else{
    //   Serial.print("Total line on file: ");
    //   Serial.println(totalLineData);
    // }

    if (WiFi.status() != WL_CONNECTED){
      Serial.println("Interlock! Save Data\n");
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
      // saveData(SD, "/data.txt", totalLineData);
      countSaveData += 1;
      String temp_dataMessage = String(countSaveData) + "," + dataMessage;

      //Append the data to backup file
      appendFile(SD, "/data_backup.txt", temp_dataMessage.c_str());
      Serial.printf("Total Stored Data: %d\n", countSaveData);
    }
    else{
      Serial.println("Send Data to AWS IOT Cloud");
    }

    // readFile(SD, "/data.txt");

    lastTime = millis();
  } 
  else if (WiFi.status() != WL_CONNECTED){
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

    delay(5000);
  }
  else{
    tft.fillScreen(TFT_BLACK);
  }

  client.loop();
  delay(5000);
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

// Print the header for a display screen
void header(const char *string)
{
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA, TFT_BLUE);
  tft.fillRect(0, 0, 480, 30, TFT_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(string, 239, 2, 4); // Font 4 for fast drawing with background
}

// Draw a + mark centred on x,y
void drawDatum(int x, int y)
{
  tft.drawLine(x - 5, y, x + 5, y, TFT_GREEN);
  tft.drawLine(x, y - 5, x, y + 5, TFT_GREEN);
}


// There follows a crude way of flagging that this example sketch needs fonts which
// have not been enabled in the User_Setup.h file inside the TFT_HX8357 library.
//
// These lines produce errors during compile time if settings in User_Setup are not correct
//
// The error will be "does not name a type" but ignore this and read the text between ''
// it will indicate which font or feature needs to be enabled
//
// Either delete all the following lines if you do not want warnings, or change the lines
// to suit your sketch modifications.

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
