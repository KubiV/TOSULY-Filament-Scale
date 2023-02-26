#include <U8glib.h>  // U8glib library

U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);

const unsigned int MAX_MESSAGE_LENGTH = 12;
String drawing = "0";

void draw(String input) 
{
   u8g.setFont(u8g_font_fub17r);   // select font
   u8g.setPrintPos(0, 20);
   u8g.print(input);  // display temperature from DHT11 in Celsius
   //u8g.println(" g"); 
}
void setup(void) 
{
  // flip screen, if required
  // u8g.setRot180();

  Serial.begin(9600);
}
void loop(void)
{

 //Check to see if anything is available in the serial receive buffer
 while (Serial.available() > 0)
 {
   //Create a place to hold the incoming message
   static char message[MAX_MESSAGE_LENGTH];
   static unsigned int message_pos = 0;

   //Read the next available byte in the serial receive buffer
   char inByte = Serial.read();

   //Message coming in (check not terminating character) and guard for over message size
   if ( inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1) )
   {
     //Add the incoming byte to our message
     message[message_pos] = inByte;
     message_pos++;
   }
   //Full message received...
   else
   {
     //Add null character to string
     message[message_pos] = '\0';

     //Print the message (or do other things)
     Serial.println(message);
     drawing = message;
     //Reset for the next message
     message_pos = 0;
   }
 }
  
   u8g.firstPage();  
   do {
   draw(drawing);
   } while( u8g.nextPage() );
      delay(500);
}
