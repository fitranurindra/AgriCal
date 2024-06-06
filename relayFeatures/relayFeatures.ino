#include <TFT_eSPI.h> // Hardware-specific library
#include "Free_Fonts.h"

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define FSB12 &FreeSerifBold12pt7b

#define UPDATE_BUTTON_PIN 16
#define EMERGENCY_BUTTON_PIN 13
#define EMERGENCY_RELAY_PIN 26
#define SLEEP_RELAY_PIN 25

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

TFT_eSPI tft = TFT_eSPI(); 

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
  tftStartDisplay();
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
    tftDisplayData1();
  }
  else if (((millis() - lastDisplayTime) > delayPageDisplay) && ((millis() - lastDisplayTime) <= 2*delayPageDisplay)){
    tftDisplayData2();
  }
  else if (((millis() - lastDisplayTime) > 2*delayPageDisplay) && ((millis() - lastDisplayTime) <= 3*delayPageDisplay)){
    tftDisplayData1();
  }
  else if (((millis() - lastDisplayTime) > 3*delayPageDisplay) && ((millis() - lastDisplayTime) <= 4*delayPageDisplay)){
    tftDisplayData2();
  }
  else if (((millis() - lastDisplayTime) > 4*delayPageDisplay) && ((millis() - lastDisplayTime) <= 5*delayPageDisplay)){
    tftDisplayData1();
  }
  else if (((millis() - lastDisplayTime) > 5*delayPageDisplay) && ((millis() - lastDisplayTime) <= 6*delayPageDisplay)){
    tftDisplayData2();
  }
}

// Print the header for a display screen
void header(const char *string){
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA, TFT_BLUE);
  tft.fillRect(0, 0, 480, 30, TFT_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(string, 239, 2, 4); // Font 4 for fast drawing with background
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

void tftDisplayData1(){
  int xpos =  0;
  int ypos = 40;

  tft.fillScreen(TFT_NAVY); // Clear screen to navy background

  header("AgriCal (Data 1)");

  // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
  // even if we specify it.
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
  // tft.setFreeFont(&FreeSerif9pt7b);
  tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
  // as the datum, so we must move the cursor down a line from the 0,0 position
  tft.print("humidity (%): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Temperature (°C): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("Pressure (mbar): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 24 point font
  tft.println();                // Move cursor down a line
  tft.print("Lux (lx): ");  // Print the font name onto the TFT screen
}

void tftDisplayData2(){
  int xpos =  0;
  int ypos = 40;

  tft.fillScreen(TFT_NAVY); // Clear screen to navy background

  header("AgriCal (Data 2)");

  // For comaptibility with Adafruit_GFX library the text background is not plotted when using the print class
  // even if we specify it.
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(xpos, ypos);    // Set cursor near top left corner of screen

  tft.setFreeFont(FSB12);   // Select Free Serif 9 point font, could use:
  // tft.setFreeFont(&FreeSerif9pt7b);
  tft.println();          // Free fonts plot with the baseline (imaginary line the letter A would sit on)
  // as the datum, so we must move the cursor down a line from the 0,0 position
  tft.print("windSpeed (km/h): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("windDirection (°): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 12 point font
  tft.println();                // Move cursor down a line
  tft.print("windGust (km/h): ");  // Print the font name onto the TFT screen

  tft.setFreeFont(FSB12);       // Select Free Serif 24 point font
  tft.println();                // Move cursor down a line
  tft.print("rainAmmount (l): ");  // Print the font name onto the TFT screen
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

