#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "WiFi.h"

#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Libraries for DHT
#include "DHT.h"

#define DHTTYPE DHT11
#define DHTPIN 17

#define UPDATE_BUTTON_PIN 16
#define EMERGENCY_BUTTON_PIN 0

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);

// // Replace with your network credentials
// const char* ssid     = "unga";
// const char* password = "punyabunga";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 120000;

// Variables to hold sensor readings
float h;
float t;
float f;
float hic;
float hif;

String dataMessage;

// int updateButton;            // the current reading from the input pin
// int lastButtonState = LOW;  // the previous reading from the input pin

// // the following variables are unsigned longs because the time, measured in
// // milliseconds, will quickly become a bigger number than can be stored in an int.
// unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
// unsigned long debounceDelay = 50;    // the debounce time; increase if the 

volatile bool isDisplay = false;
// volatile bool lastDisplayState = false;
unsigned long lastDisplayTime = 0;  // the last time the output pin was toggled
unsigned long displayDelay = 60000;    // the debounce time; increase if the 
unsigned long delayPageDisplay = 10000; 

volatile bool manualUpdate = false;
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

  dht.begin();

  pinMode(UPDATE_BUTTON_PIN, INPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
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
  
  attachInterrupt(digitalPinToInterrupt(UPDATE_BUTTON_PIN), triggerManualUpdate, FALLING);

  display.clearDisplay();
  display.setTextSize(2.5);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  // Display static text
  display.println("Agrical");
  display.display();
  delay(5000);

  Serial.println();
  Serial.println("Program Started");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED){
    if(!client.connect(THINGNAME)){
      connectAWS();
    }
  }

  if ((millis() - lastTime) > timerDelay) {
    dhtUpdate();

    //Concatenate all info separated by commas
    dataMessage = String(h) + "," + String(t) + "," + String(f) + "," + String(hic) + "," + String(hif) + "\r\n";
    Serial.printf("\nSaving data to SD Card: ");
    Serial.print(dataMessage);

    isDisplay = true;
    lastDisplayTime = millis();

    if (WiFi.status() != WL_CONNECTED){
      Serial.println("Interlock! Save Data\n");
    }
    else{
      publishMessage();
      Serial.println("Send Data to AWS IOT Cloud");
    }

    lastTime = millis();
  }
  else if(manualUpdate){
    manualUpdate = false;
    dhtUpdate();

    //Concatenate all info separated by commas
    dataMessage = String(h) + "," + String(t) + "," + String(f) + "," + String(hic) + "," + String(hif) + "\r\n";
    Serial.printf("\nSaving data to SD Card: ");
    Serial.print(dataMessage);

    isDisplay = true;
    lastDisplayTime = millis();

    if (WiFi.status() != WL_CONNECTED){
      Serial.println("Interlock! Save Data\n");
    }
    else{
      publishMessage();
      Serial.println("Send Data to AWS IOT Cloud");
    }
  }
  else if (WiFi.status() != WL_CONNECTED){
    display.clearDisplay();
    display.setTextSize(2.5);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    display.println("Reconnecting");
    display.display();
  }
  else if (!isDisplay){
    display.clearDisplay();
    display.display();
    Serial.println("No Display");
  }

  if (isDisplay){
    displayPage();
  }

  // lastDisplayState = isDisplay;

  client.loop();
  delay(50);
}

void displayPage(){
  // if (isDisplay != lastDisplayState) {
  //   lastDisplayTime = millis();
  // }
  if ((millis() - lastDisplayTime) > displayDelay) {
    isDisplay = false;
  }
  else if ((millis() - lastDisplayTime) <= delayPageDisplay){
    oledDisplay();
    Serial.println("Page 1");
  }
  else if (((millis() - lastDisplayTime) > delayPageDisplay) && ((millis() - lastDisplayTime) <= 2*delayPageDisplay)){
    display.clearDisplay();
    display.setTextSize(2.5);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    display.println("Page 2");
    display.display();

    Serial.println("Page 2");
  }
  else if (((millis() - lastDisplayTime) > 2*delayPageDisplay) && ((millis() - lastDisplayTime) <= 3*delayPageDisplay)){
    oledDisplay();
    Serial.println("Page 1");
  }
  else if (((millis() - lastDisplayTime) > 3*delayPageDisplay) && ((millis() - lastDisplayTime) <= 4*delayPageDisplay)){
    display.clearDisplay();
    display.setTextSize(2.5);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    display.println("Page 2");
    display.display();

    Serial.println("Page 2");
  }
  else if (((millis() - lastDisplayTime) > 4*delayPageDisplay) && ((millis() - lastDisplayTime) <= 5*delayPageDisplay)){
    oledDisplay();
    Serial.println("Page 1");
  }
  else if (((millis() - lastDisplayTime) > 5*delayPageDisplay) && ((millis() - lastDisplayTime) <= 6*delayPageDisplay)){
    display.clearDisplay();
    display.setTextSize(2.5);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    display.println("Page 2");
    display.display();

    Serial.println("Page 2");
  }
}

void triggerManualUpdate(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    manualUpdate = true;
  }
  last_interrupt_time = interrupt_time;

  Serial.println("Update Data Manually");
}

void oledDisplay(){
  display.clearDisplay();

  display.setTextSize(0.7);

  display.setCursor(0, 0);
  display.print("Humid: ");
  display.print(h);
  display.println(" %");

  display.setCursor(0, 8);
  display.print("Temp : ");
  display.print(t);
  display.println(" *C");

  display.setCursor(0, 16);
  display.print("Temp : ");
  display.print(f);
  display.println(" *F");

  display.setCursor(0, 24);
  display.print("Heat : ");
  display.print(hic);
  display.println(" *C");

  display.display(); 
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

