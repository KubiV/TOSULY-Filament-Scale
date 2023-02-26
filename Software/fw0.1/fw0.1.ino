#include <HX711_ADC.h> // https://github.com/olkal/HX711_ADC
#include <PinButton.h> //https://github.com/poelstra/arduino-multi-button/
#include <U8glib.h>  //https://github.com/olikraus/u8glib
#include <math.h>

#define CalibrationFactor 375 // Calibarate your LOAD CELL with 100g weight, and change the value according to readings
#define SpoolMass         250
float j = 0;

PinButton myButton(2);
HX711_ADC LoadCell(4, 5); // dt pin, sck pin

U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);
String drawing = "0";

void drawMass(String input) 
{
   u8g.setFont(u8g_font_fub17r);   // select font
   u8g.setPrintPos(0, 20);
   u8g.print(input); 
   u8g.println(" g"); 
}

void setup() {
  // flip screen, if required
  // u8g.setRot180();
  
  LoadCell.begin(); // start connection to HX711
  LoadCell.start(2000); // load cells gets 1000ms of time to stabilize
  LoadCell.setCalFactor(CalibrationFactor); 

  Serial.begin(9600);

}

void loop() {

  myButton.update();
  LoadCell.update(); // retrieves data from the load cell
  float i = LoadCell.getData(); // get output value

  if (myButton.isDoubleClick()) { //subtract defined spool mass
    Serial.println("/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////");
    j = -SpoolMass;
  } 
  
  Serial.print(round(i+j));
  Serial.println(" g");

  if (i>=5000){
    i=0;
    Serial.print("Over Loaded"); 
    delay(200);
  }
  
  if (myButton.isSingleClick()) {
    Serial.println("Taring");
    j = 0;
    LoadCell.start(2000);
  }


  u8g.firstPage();  
   do {
   drawMass(String(round(i+j)));
   } while( u8g.nextPage() );
      //delay(500);
}