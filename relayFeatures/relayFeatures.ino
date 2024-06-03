#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define UPDATE_BUTTON_PIN 16
#define EMERGENCY_BUTTON_PIN 13
#define EMERGENCY_RELAY_PIN 26
#define SLEEP_RELAY_PIN 25

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 120000;

volatile bool isEmergency = false;
volatile bool isDisplay = false;
// volatile bool lastDisplayState = false;
unsigned long lastDisplayTime = 0;  // the last time the output pin was toggled
unsigned long displayDelay = 60000;    // the debounce time; increase if the 
unsigned long delayPageDisplay = 10000; 

volatile bool manualUpdate = false;

void setup() {
  Serial.begin(115200);

  pinMode(UPDATE_BUTTON_PIN, INPUT);
  pinMode(EMERGENCY_BUTTON_PIN, INPUT);

  pinMode(EMERGENCY_RELAY_PIN, OUTPUT);
  pinMode(SLEEP_RELAY_PIN, OUTPUT);

  digitalWrite(EMERGENCY_RELAY_PIN, LOW);
  digitalWrite(SLEEP_RELAY_PIN, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  attachInterrupt(digitalPinToInterrupt(UPDATE_BUTTON_PIN), triggerManualUpdate, FALLING);
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_BUTTON_PIN), triggerEmergencyCond, FALLING);

  delay(1000);
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
  if ((millis() - lastTime) > timerDelay) {
    isDisplay = true;
    lastDisplayTime = millis();
    lastTime = millis();
  }
  else if(manualUpdate){
    Serial.println("Update Data Manually");
    manualUpdate = false;
    isDisplay = true;
    lastDisplayTime = millis();
  }
  else if (!isDisplay && !isEmergency){
    digitalWrite(SLEEP_RELAY_PIN, LOW);
    Serial.println("No Display");
  }

  if (isEmergency){
    isDisplay = false;
    digitalWrite(SLEEP_RELAY_PIN, HIGH);
    
    Serial.println("Emergency Start (Relay HIGH = ON)");
    digitalWrite(EMERGENCY_RELAY_PIN, HIGH);
  }
  else{
    Serial.println("Emergency Stop (Relay LOW = OFF)");
    digitalWrite(EMERGENCY_RELAY_PIN, LOW);
  }

  if (isDisplay){
    digitalWrite(SLEEP_RELAY_PIN, HIGH);
    displayPage();
  }

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
    oledDisplay1();
  }
  else if (((millis() - lastDisplayTime) > delayPageDisplay) && ((millis() - lastDisplayTime) <= 2*delayPageDisplay)){
    oledDisplay2();
  }
  else if (((millis() - lastDisplayTime) > 2*delayPageDisplay) && ((millis() - lastDisplayTime) <= 3*delayPageDisplay)){
    oledDisplay1();
  }
  else if (((millis() - lastDisplayTime) > 3*delayPageDisplay) && ((millis() - lastDisplayTime) <= 4*delayPageDisplay)){
    oledDisplay2();
  }
  else if (((millis() - lastDisplayTime) > 4*delayPageDisplay) && ((millis() - lastDisplayTime) <= 5*delayPageDisplay)){
    oledDisplay1();
  }
  else if (((millis() - lastDisplayTime) > 5*delayPageDisplay) && ((millis() - lastDisplayTime) <= 6*delayPageDisplay)){
    oledDisplay2();
  }
}

void oledDisplay1(){
    display.clearDisplay();
    display.setTextSize(2.5);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    display.println("Page 1");
    display.display();

    Serial.println("Page 1");
}

void oledDisplay2(){
  display.clearDisplay();
  display.setTextSize(2.5);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  // Display static text
  display.println("Page 2");
  display.display();

  Serial.println("Page 2");
}

void triggerManualUpdate(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    manualUpdate = true;
  }
  last_interrupt_time = interrupt_time;
}

void triggerEmergencyCond(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    isEmergency = !isEmergency;
  }
  last_interrupt_time = interrupt_time;

  Serial.println("Change Emergency State");
}
