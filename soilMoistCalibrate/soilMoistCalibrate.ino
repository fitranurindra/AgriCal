#define soilMoisturePin A3

const int AirValue = 2700;   //you need to replace this value with Value_1
const int WaterValue = 1320;  //you need to replace this value with Value_2

int soilMoistureValue = 0;
int soilmoisturepercent = 0;
 
 
void setup() {
  Serial.begin(115200); // open serial port, set the baud rate to 9600 bps
}

void loop() {
  soilMoistureValue = analogRead(soilMoisturePin);  //put Sensor insert into soil
  soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);

  if(soilmoisturepercent > 100){
    soilmoisturepercent = 100;
  } 
  else if(soilmoisturepercent <0) {
    soilmoisturepercent = 0;
  }
  
  Serial.println("-----------------");
  Serial.println(soilMoistureValue);
  Serial.print(soilmoisturepercent);
  Serial.println("%");
  Serial.println("-----------------");
  
  delay(1000);
}

// void soilMoisture_update(){
//   // soilMoistureValue = analogRead(soilMoisturePin);
//   soilMoistureValue = analogRead(soilMoisturePin)*470/2480;
//   moisturePercentage = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
// }