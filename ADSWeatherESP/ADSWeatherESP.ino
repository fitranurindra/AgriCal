#include "ADSWeatherV2.h"

#define RAIN_PIN 33
#define WIND_DIR_PIN A0
#define WIND_SPD_PIN 27

ADSWeatherV2 weatherStation(RAIN_PIN, WIND_DIR_PIN, WIND_SPD_PIN);

unsigned long last_update;
unsigned long delay_update = 5000;

void setup() {
  Serial.begin(115200);
  pinMode(WIND_DIR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), weatherStation.countRain, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIND_SPD_PIN), weatherStation.countAnemometer, FALLING);
}

void loop() {
  weatherStation.update();
  int adc = analogRead(WIND_DIR_PIN);

  if (millis() - last_update > delay_update){
    Serial.println(adc);
    Serial.print("Rain: ");
    Serial.println(weatherStation.getRain());
    
    Serial.print("Wind Speed: ");
    Serial.println(weatherStation.getWindSpeed());
    
    Serial.print("Wind Direction: ");
    Serial.println(weatherStation.getWindDirection());
    
    Serial.print("Wind Gust: ");
    Serial.println(weatherStation.getWindGust());

    Serial.println("------------------");
    last_update = millis();
  }

  delay(100);
}


// #define windDir A0 // pin tied to Vout for voltage divider circuit

// // These arrays are specific to the ADS wind vane with a 10kOhm fixed resistor in the voltage divider
// int sensorExp[] = {130
// ,190
// ,230
// ,330
// ,580
// ,830
// ,980
// ,1700
// ,1845
// ,2260
// ,2400
// ,2680
// ,3000
// ,3230
// ,3550
// ,3800
// };
// float dirDeg[] = {112.5,67.5,90,157.5,135,202.5,180,22.5,45,247.5,225,337.5,0,292.5,315,270};
// char* dirCard[] = {"ESE","ENE","E","SSE","SE","SSW","S","NNE","NE","WSW","SW","NNW","N","WNW","NW","W"};

// int sensorMin[] = {0,170,220,310,490,232,273,385,438,569,613,667,746,812,869,931};
// int sensorMax[] = {150,210,300,450,7,257,301,426,484,612,661,737,811,868,930,993};

// int incoming = 0;
// float angle = 0;

// void setup() {
// Serial.begin(9600);
// }

// void loop() {
//   incoming = analogRead(windDir);
//   for(int i=0; i<=15; i++) {
//    if(incoming >= sensorMin[i] && incoming <= sensorMax[i]) {
//     angle = dirDeg[i];
//     break;
//    } 
//   }
  
//   Serial.print(angle);
//   Serial.print(":");
//   delay(25);
// }
