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

unsigned long nextCalc;
unsigned long timer;

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

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

ADSWeather ws1(RAIN_PIN, VANE_PIN, ANEMOMETER_PIN); //This should configure all pins correctly
Adafruit_BME280 bme;

BH1750 lightMeter;

SoftwareSerial nodemcu(9,10);

// StaticJsonDocument<300> doc;

// /*
//  * JSON Data Format
// */
// const size_t capacity = JSON_OBJECT_SIZE(7);
// DynamicJsonDocument doc(capacity);

void connectAWS()
{
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

void publishMessage()
{
  StaticJsonDocument<200> doc;
  //Assign collected data to JSON Object
  doc["voltage"] = voltage;

  doc["lux"] = lux;
  doc["solarRadiation"] = solarRadiation;

  doc["moisture"] = moisturePercentage;

  doc["humidity"] = humBME;
  doc["temperature"] = tempBME;
  doc["pressure"] = pressBME;

  doc["windSpeed"] = windSpeed;
  doc["windDirection"] = windGust;
  doc["windGust"] = windDirection;
  doc["rainAmmount"] = rainAmmount;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(char* topic, byte* payload, unsigned int length)
{
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

void setup() {
  Serial.begin(115200);
  nodemcu.begin(115200);
  connectAWS();

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
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
  nextCalc = millis() + CALC_INTERVAL;

  Serial.println("Program started");
}

void loop() {
  timer = millis();

  //Update Data Every Cycle
  bme_update();
  ws1.update();
  weather_update();

  // soilMoistureValue = analogRead(soilMoisturePin);
  soilMoistureValue = analogRead(soilMoisturePin)*470/2480;
  moisturePercentage = map(soilMoistureValue, AirValue, WaterValue, 0, 100);

  lux = lightMeter.readLightLevel();
  solarRadiation = (lux*0.0079);

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

  // //Assign collected data to JSON Object
  // doc["lux"] = lux;
  // doc["solarRadiation"] = solarRadiation;

  // doc["moisture"] = moisturePercentage;

  // doc["humidity"] = humBME;
  // doc["temperature"] = tempBME;
  // doc["pressure"] = pressBME;

  // doc["windSpeed"] = windSpeed;
  // doc["windDirection"] = windGust;
  // doc["windGust"] = windDirection;
  // doc["rainAmmount"] = rainAmmount;

  publishMessage();
  client.loop();

  //Send data to NodeMCU
  // serializeJson(doc, nodemcu);
  // doc.clear();

  delay(10000);
}
