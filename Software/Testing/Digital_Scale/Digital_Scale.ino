//Arduino Digital Weight Scale HX711 Load Cell Module
#include <HX711_ADC.h> // https://github.com/olkal/HX711_ADC
#include <PinButton.h> //https://github.com/poelstra/arduino-multi-button/

#define CalibrationFactor 375 // Calibarate your LOAD CELL with 100g weight, and change the value according to readings
#define SpoolMass         250

PinButton myButton(2);
HX711_ADC LoadCell(4, 5); // dt pin, sck pin

void setup() {
  
  LoadCell.begin(); // start connection to HX711
  LoadCell.start(1000); // load cells gets 1000ms of time to stabilize
  LoadCell.setCalFactor(CalibrationFactor); 
  
  Serial.begin(9600);
}

void loop() { 

  myButton.update();
  LoadCell.update(); // retrieves data from the load cell
  float i = LoadCell.getData(); // get output value
  
  Serial.print(i);
  Serial.println(" g");

  if (i>=5000){
    i=0;
    Serial.print("Over Loaded"); 
    delay(200);
  }
  
  if (myButton.isSingleClick()) {
    Serial.println("Taring");
    LoadCell.start(1000);
  }

  if (myButton.isDoubleClick()) {
    Serial.println("/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
  } 
  
}
