//Include Lib for Arduino to Nodemcu
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>

// #define ss 15
// #define rst 4
// #define dio0 2

//D3 = Rx & D4 = Tx
SoftwareSerial nodemcu(3, 4);

// const size_t capacity = JSON_OBJECT_SIZE(7);
// DynamicJsonDocument doc(capacity);

StaticJsonDocument<300> doc;

//Timer to run Arduino code every 5 seconds
unsigned long previousMillis = 0;
unsigned long currentMillis;
const unsigned long period = 5000; 

float lux;
float solarRadiation;

float tempBME;
float humBME;
float pressBME;

float moisture;

float rainAmmount;
float windSpeed;
int windDirection;
float windGust;

void setup() {
  // Initialize Serial port
  Serial.begin(9600);
  nodemcu.begin(9600);
  while (!Serial) continue;

  // Serial.println("LoRa Sender");
  // LoRa.setPins(ss, rst, dio0);
  // // if (!LoRa.begin(433E6)) {
  // //   Serial.println("Starting LoRa failed!");
  // //   while (1);
  // // }
}

void loop() {
  //Get current time
  currentMillis = millis();
  DeserializationError error = deserializeJson(doc, nodemcu);

  // // Test parsing
  // while (error) {
  //   Serial.println("Invalid JSON Object");
  //   delay(500);
  //   DeserializationError error = deserializeJson(doc, nodemcu);
  // }

  if ((currentMillis - previousMillis >= period)) {
    // Serial.println("Sending packet: ");
    Serial.println("JSON Object Recieved");
    lux = doc["lux"];
    solarRadiation = doc["solarRadiation"];
    moisture = doc["moisture"];
    humBME = doc["humidity"];
    tempBME = doc["temperature"];
    pressBME = doc["pressure"];
    windSpeed = doc["windSpeed"];
    windGust = doc["windDirection"];
    windDirection = doc["windGust"];
    rainAmmount = doc["rainAmmount"];

    // LoRa.beginPacket();
    // LoRa.print(moisture);
    // LoRa.endPacket();

    Serial.print("Recieved Lux:  ");
    Serial.println(lux);

    Serial.print("Recieved Solar Radiation:  ");
    Serial.println(solarRadiation);

    Serial.print("Recieved Moisture:  ");
    Serial.println(moisture);

    Serial.print("Recieved Humidity:  ");
    Serial.println(humBME);

    Serial.print("Recieved Temperature:  ");
    Serial.println(tempBME);

    Serial.print("Recieved Pressure:  ");
    Serial.println(pressBME);

    Serial.print("Recieved Wind Speed:  ");
    Serial.println(windSpeed);

    Serial.print("Recieved Gusting at:  ");
    Serial.println(windGust);

    Serial.print("Recieved Wind Direction:  ");
    Serial.println(windDirection);

    Serial.print("Recieved Rain Ammount:  ");
    Serial.println(rainAmmount);

    Serial.println("-----------------------------------------");

    previousMillis = previousMillis + period;
  }
}