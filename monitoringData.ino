#include "secrets.h"
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

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);


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

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int update_mode = 1;
volatile bool isUpdate = false;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

ADSWeather ws1(RAIN_PIN, VANE_PIN, ANEMOMETER_PIN); //This should configure all pins correctly
Adafruit_BME280 bme;

BH1750 lightMeter;

SoftwareSerial nodemcu(9,10);

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  nodemcu.begin(115200);
  connectAWS();

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

  delay(1000);
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

  Serial.println("Program started");
}

void loop() {
  int amtUpdate = xUpdate(update_mode);
  int* time_update = (int*) malloc(amtUpdate * sizeof(int)); 
  updateTime(time_update, update_mode); 

  DateTime now = rtc.now();

  for (int i = 0; i < amtUpdate; i++){
    if(now.minute() == time_update[i] && now.second() < 31){
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

    // Update Weather Data
    bme_update();
    ws1.update();
    weather_update();
    voltage_update();
    soilMoisture_update();
    lux_update();

    serialMonitor();

    oledDisplay();

    publishMessage();
  }
  else{
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

  formattedDate = temp_a + "/" + temp_b + "/" + temp_c;

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
  display.println(" *C");

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
  Serial.println("*C");

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
  Serial.println(" degree");

  Serial.print("Total Rain: ");
  Serial.print(rainAmmount);
  Serial.println(" liter");

  Serial.println("-------------------------------");
}

void connectAWS(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

void publishMessage(){
  StaticJsonDocument<200> doc;
  //Assign collected data to JSON Object
  DateTime nowDT = rtc.now();
  String date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
  String time = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
  doc["date"] = date;
  doc["time"] = time;
  
  doc["voltage"] = voltage;

  doc["lux"] = lux;
  doc["solarRadiation"] = solarRadiation;

  doc["moisture"] = moisturePercentage;

  doc["humidity"] = humBME;
  doc["temperature"] = tempBME;
  doc["pressure"] = pressBME;

  doc["windSpeed"] = windSpeed;
  doc["windDirection"] = windDirection;
  doc["windGust"] = windGust;
  doc["rainAmmount"] = rainAmmount;
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
  String date = formatDate(nowDT.day(), nowDT.month(), nowDT.year());
  Serial.print(date); Serial.print(" ==> ");
}

void showTime(){
  DateTime nowDT = rtc.now();
  String time = formatTime(nowDT.hour(), nowDT.minute(), nowDT.second());
  Serial.println(time);
}

void updateTime(int* time_update, int update_mode){  
  if (update_mode == 0){ // Update Data Setiap 30 Menit
    time_update[0] = 0;
    time_update[1] = 30;
  }
  else if (update_mode == 1){ // Update Data Setiap 15 Menit
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
