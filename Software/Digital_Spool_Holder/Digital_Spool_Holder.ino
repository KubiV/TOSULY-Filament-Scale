
//SOURCE: https://www.instructables.com/Digital-Spool-Holder-with-Scale/

/*
Digital Spool Holder

This sketch is for a scale made to measure the weight (g) of the
remaining material in a spool. It was made mainly for 3D printers but it can be
used for any kind of spool or container.

The main difference between this and a common scale is that this device has the
possibility to add multiple profiles to store different Tare values
so that way we can measure only the weight of the remaining filament excluding
the weight of the empty spool, because each brand of filaments has a different
weight and material for the spool itself.

It uses a load cell connected to a HX711 module.

We also use an 1.3" OLED display 128x64 I2C with SSD1306 driver using the
U8GLIB library. It also works on a 0.96" OLD display of the same characteristics
but everything would look smaller so I don't recommend it.


Features:
 * This sketch can measure weights from 10g to 9999g.
 * There are 3 buttons (LEFT, ENTER, RIGHT) that can be used for scrolling 
   through different profiles and navigate through the menu system.
 * You can add, edit and delete profiles using the menu system.
   It can store up to 20 profiles, with their own name and Tare value.
   Maximum 21 characters for the name of each profile.
 * You can do a full calibration for the load cell using the menu system, 
   so there's no need to edit this sketch in any way.
 * Configurable deadzone with real-time reading on the display. The deadzone
   helps ignoring the fluctuations of the constant pulling/pushing of the
   filament in the 3D printer.
 * All the settings are stored in the EEPROM so it's going to remember all the 
   settings after powering the device off.
 * There is an option in the menu to FACTORY RESET arduino for clearing the EEPROM (user data).
 * Auto-center the tittle text in the main/normal screen ignoring spaces to the right.
 * When adding a new profile, it's going to save the new profile after the current 
   profile so the order can be arranged in a predictable way. To make this work it
   required a lot of effort so even though you might take this for granted, it's not
   a trivial feature.
 * In the menu you can set the amount of deadzone to ignore the peaks when the 3D printer 
   pulls the filament, so we show the most realistic estimate of the actual weight left in the spool.
 * When powering ON, changing profiles or returning from the menu, it shows the weight 
   starting at the lower edge of the deadzone so the device is ready to measure the decrease of weight
   of the filament immediately.
 * No timeout for being on the menu. Yes, that's right, this is a feature. Why? I hate timeouts
   that will exit the menu if no operation is done after a few seconds, but I think that is
   a bad design that causes stress and frustration because you feel like you need to rush things
   with the threat of having to start all over from the beginning. You are not going to find anything
   like that in here.


More details about the project in here: https://www.instructables.com/Digital-Spool-Holder-with-Scale/

Pins for HX711 module to arduino:
 * GND         = GND
 * VCC         = 5V
 * DT (DATA)   = 2
 * SCK (CLOCK) = 3

Load cell connection to the HX711 module:
 * RED   = E+
 * BLACK = E-
 * WHITE = A-
 * GREEN = A+

To control everything we use 3 buttons:
 * LEFT ARROW  = Pin 5: Scroll to the left.
 * ENTER       = Pin 6: This button is to enter the menu and select options.
 * RIGHT ARROW = Pin 7: Scroll to the right.
Note: When you press the button, the pin should receive GROUND, and when you
release the button the pin should receive 5V. We use the internal pullup
resistor for this but I recommend adding an aditional 10K resistor to each pin
connected to each button pin and 5V. This way is going to be more reliable.
I say this because I've seen the internal pullup resistor fail after some time.

Pins for OLED Display:
 * GND = GND
 * VCC = 5V
 * SCL = A5
 * SDA = A4

It's a good idea to put a resistor (3.3K) between A4-5V and A5-5V to help stabilize
the connection. What that does is to pull-up the I2C pins to make it more reliable
and prevents lock-ups (yes, this can happen without adding resistors).


HX711 maximum raw value allowed: 8388607

Load cell actual maximum weight reached:
 * 2Kg = 8.4Kg


Reference of weight for empty spools:
 * Plastic = 244g
 * Cardboard = 177g


Note: When exposing the cell to very heavy weights, more than the rating,
the cell permanetly bends a little so may require Tare. Also, with time this
can happen but only very little (around 3g).


Ucglib library: https://github.com/olikraus/ucglib
HX711 library: https://github.com/bogde/HX711


User Reference Manual: https://github.com/olikraus/u8glib/wiki/userreference
List of fonts: https://github.com/olikraus/u8glib/wiki/fontsize

Sketch made by: InterlinkKnight
Last modification: 2022/12/18
*/





// Pins:
const byte LeftButtonPin = 7;  // Pin for LEFT button
const byte EnterButtonPin = 6;  // Pin for ENTER button
const byte RightButtonPin = 5;  // Pin for RIGHT button


#include "U8glib.h"  // Include U8glib library for the display

// Create display and set pins:
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_FAST);  // Dev 0, Fast I2C / TWI


#include "HX711.h"  // Include HX711 library for the load cell

// Set pins for HX711 module:
const byte LOADCELL_DOUT_PIN = 2;
const byte LOADCELL_SCK_PIN = 3;

HX711 scale;  // Create object for the load cell and give it a name


#include "EEPROM.h"  // Include EEPROM library to be able to store settings





///////////////
// ADJUSTMENTS:
///////////////
// The following items are the values that you could want to change:

// Smoothing the weight:
const byte numReadings1 = 20;  // Number of samples for smoothing. The more samples, the smoother the value
                               // although that will delay refreshing the value. 

const byte MinimumWeightAllowed = 10;  // When the weight is below this value, we are going to consider that the weight is actually 0 so we show a 0


// Debouncing buttons:
const byte ButtonDebouncingAmount = 2;  // Debouncing amount (cycles) for how long to wait. The higher, the more
                                        // it will wait to see if button remain release to really consider is released.
                                        // It adds a lag to make sure we really release the button, ignoring micro pulses.

// Holding buttons:
const byte RepeatWhenWeHoldAmount = 15;  // For how long (cycles) to wait when holding the button, before considering is holded
const byte RepeatWhenWeHoldFrequencyDelay = 0;  // How often (cycles) repeat pulses when we hold the button.
                                // 0 = one cycle pulse and one cycle no (101010101...)
                                // 1 = pulse every 2 cycles (1001001001...)
                                // 2 = pulse every 3 cycles (1000100010001...)

const byte WeightYPosition = 50;  // The Y position of the weight when in normal screen
const byte SelectBoxWigth = 55;  // This is how wide the selection box is when is in the center
const byte SelectBoxHeight = 12;  // This is how high the selection box is

const byte SelectBoxXPositionRight = 73;  // X position of the selection box that goes to the right
const byte SelectBoxYPosition = 52;  // Y position of the selection box
const byte SelectBoxXPositionCenter = 36;  // X position of the selection box that goes to the center

const byte ProgressBarSpeedAmount = 5;  // How many cycles to wait to increase the progress bar by 1 pixel.
                                        // The higher, the slower the bar will increase.

// Text Cursor:
const byte BlinkingCursorAmount = 5;  // How many cycles go between blincking the cursor when editing text.
                                      // The higher, the slower is going to do the blinking.





/////////////
// Variables:
/////////////

// Values set in the full calibration process:
long EmptyHolderRawValue;  // The raw value when nothing is in the holder. Set in the full calibration
int EmptySpoolMeasuredWeight;  // The weight when the holder has an empty spool

long FullSpoolRawValue;  // The raw value when a full spool is in the holder. Set in the full calibration
long FullSpoolWeight;  // Weight (g) that a full spool has when calibrating. Set in the full calibration


byte VariableCalibrationDone;  // Stores the readed value of the EEPROM to know if we have done the full calibration before
                               // 0 = Never done before
                               // 1 = Full calibration has been done before

// Deadzone to the weight:
byte DeadzoneSamples = 0;  // Dead zone amount. This will update the shown value only if the reading is below
                           // or greater than the current shown value by a set amount.
                           // 0 means no deadzone.
                           // 1 means the new value have to go over last value by 2 to refresh.
                           // This is set by the user in the menu system.

byte ProgressBarCounter;  // Counter for progress bar
byte ProgressBarSpeedCounter;  // The counter for the speed of the progress bar


byte WeightXPosition;  // X position to display the weight. It depends on the amount of digits so I later change this depending on that

long FullCalibrationRawValueEmptyHolder;  // EEPROM with calibrated zero raw value

byte BlinkingCursorCounter;  // Counter for blincking the cursor when editing text
byte BlinkingCursorState = 1;  // State for blincking the cursor when editing text

long RawLoadCellReading;  // Raw reading from load cell
long HolderWeight;  // Current weight converted to grams, based on calibration
long CurrentProfileWeightShown;  // This variable stores the weight minus the weight of the empty spool on the current profile
long SmoothedWeight;  // Weight with smooth so is more stable
long SmoothedWeightToShowAsInputInDeadzone;  // When we are in deadzone selection, this is the input is going to show

char CurrentProfileTitle[22];  // Character array for current profile name
char ProfileTitleTemporal[22] = "PROFILE 1";  // Character array for profile name before checking the amount of characters.
                                              // The current profile name is stored here, readed from EEPROM
byte AmountOfCharProfileName = 5;  // Stores the temporal amount of characters
                                   // Maximum 21 characters supported

byte DeadzoneAmountSet = 1;  // Amount of deadzone set by the user
long WeightWithDeadzone;  // Weight with deadzone
long FinalWeightToShow;

// Variables for smoothing weight:
long readings1[numReadings1];  // The input
int readIndex1;  // The index of the current reading
long total1;  // The running total

// Variables for debouncing button setting:
byte ButtonDebouncingLastState;  // Stores last state of the button
byte ButtonDebouncingCounter;  // Counter for how long to wait before considering button was really release


// Variables for Enter button:
byte ButtonEnterDebouncingCounter;
byte ButtonEnterDebouncingLastState;
byte EnterButtonActivationStateWithDebounce;
byte EnterButtonHoldRepeatCounter;
byte EnterButtonHoldCounter;
byte EnterButtonActivationStatePulses;
byte EnterButtonActivationStateLongHold;
byte EnterButtonRepeatWhenWeHoldFrequencyCounter;

byte EnterButton1PulseOnly;  // When you press a button, this variable is going
                             // to be value 1 only one cycle. The next cycle is going to be 0.
                             // This is to avoid exceuting the button action continuesly.
                             // I want to do the aciton only one cycle.

byte EnterRepeatWhenWeHoldFrequencyCounter;  // Counter to know how often create repeat pulses when holding the button


// Variables for right button:
byte ButtonRightDebouncingLastState;  // Stores last state of the button
byte ButtonRightDebouncingCounter;  // Counter for how long to wait before considering button was really release

// There are many places where we use the Right button, so instead of repeating the debouncing all the time,
// I read the button and debounce ones and record the state of the button to the following variable.
// If the variable is 1, we excecute whatever we need to do when we press the button.
byte RightButtonActivationStateWithDebounce;  // Stores if we are pressing the button, with debounce

byte RightButtonActivationStatePulses;  // Stores state of (+) button pulses
byte RightButtonActivationStateLongHold;  // Stores if we are long-holding the button. Goes to 1 when we are holding
byte RightButtonRepeatWhenWeHoldFrequencyCounter;  // Counter to know how often create repeat pulses when holding the button
byte RightButtonHoldRepeatCounter;  // Counts for how long we are holding the Right button

// Variable to record if Right button action was already done so we don't excecute commands continuosly:
byte RightButtonHoldCounter;

byte RightButton1PulseOnly;  // Variable to store one pulse only for Right button

// Variables for Left button:
byte ButtonLeftDebouncingLastState;  // Stores last state of the button
byte ButtonLeftDebouncingCounter;  // Counter for how long to wait before considering button was really release

// There are many places where we use the Left button, so instead of repeating the debouncing all the time,
// I read the button and debounce ones and record the state of the button to the following variable.
// If the variable is 1, we excecute whatever we need to do when we press the button.
byte LeftButtonActivationStateWithDebounce;  // Stores if we are pressing the button, with debounce

byte LeftButtonActivationStatePulses;  // Stores state of (+) button pulses
byte LeftButtonActivationStateLongHold;  // Stores if we are long-holding the button. Goes to 1 when we are holding
byte LeftRepeatWhenWeHoldFrequencyCounter;  // Counter to know how often create repeat pulses when holding the button
byte LeftButtonHoldRepeatCounter;  // Counts for how long we are holding the Left button

// Variable to record if Left button action was already done so we don't excecute commands continuosly:
byte LeftButtonHoldCounter;

byte LeftButton1PulseOnly;  // Variable to store one pulse only for Left button

byte TitleXPosition;  // Variable to store the X position for printing profile title.
                    // This change depending on the amount of characters to center the text.

byte gSymbolXPosition;  // X position of the g symbol. Depends on the amount of digits of the weight

int ScreenPage;  // Page of the display

byte CounterForWhileEEPROMSectorState;  // I use this in a while function to go through all sectors of EEPROM and see how many are available
  
byte SelectorPosition;  // Stores the position of the cursor to select an option

byte ArrayCounterForCheckingSectorsNewProfile;  // A counter for a while to check what's the next sector available, when adding a new profile

byte CurrentProfileEEPROMSector;  // The profile number that its currently on
byte CurrentProfileUserSlot;  // The profile slot position for the user
byte UserProfileSlotSequence[20] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};  // This is an array to store the EEPROM memory number into a sequence for the user so it can navigate the profiles in the correct order
                                // Each one saves what channel to use in the EEPROM.
                                // By default I have it to correspond to the EEPROM channels, but when people add new profiles, the slot of the next one might require to move it forward to make space for the new one.
                                // The reason is that I want the new profile to always be next to the current profile.
                                // If I would use the EEPROM channel as is and the next profile has already data, then the new profile would not be next to the current profile.

// Adding new profile:
byte TemporalCurrentProfileUserSlot;  // To use in a while loop to check next user profile slot available

byte TemporalSlotAvailable;  // Profile slot we are going to write with the previous used profile. This is used when we copy/move slots when adding a new profile
byte TemporalSlotToBeCopy;  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile
 
byte TargetUserProfileSlot;  // Stores the target user profile slot that we are trying to get available when creating a new profile
byte SectorThatNowIsAvailable;  // When adding a new profile, it stores the EEPROM sector that is now available, so we can later move other sectors to here

// Constant Text in PROGMEM:
// The follwing list is all the text is going to be printed in the display that
// is static (never changes).
// The reason of putting it here is so it doesn't get stored in the Dynamic Memmory (RAM).
// Using the funtion PROGMEM force using only the flash memory.
const char ABCTextCharacterA[14] PROGMEM       = "      A B C D";
const char ABCTextCharacterB[14] PROGMEM       = "    A B C D E";
const char ABCTextCharacterC[14] PROGMEM       = "  A B C D E F";
const char ABCTextCharacterD[14] PROGMEM       = "A B C D E F G";
const char ABCTextCharacterE[14] PROGMEM       = "B C D E F G H";
const char ABCTextCharacterF[14] PROGMEM       = "C D E F G H I";
const char ABCTextCharacterG[14] PROGMEM       = "D E F G H I J";
const char ABCTextCharacterH[14] PROGMEM       = "E F G H I J K";
const char ABCTextCharacterI[14] PROGMEM       = "F G H I J K L";
const char ABCTextCharacterJ[14] PROGMEM       = "G H I J K L M";
const char ABCTextCharacterK[14] PROGMEM       = "H I J K L M N";
const char ABCTextCharacterL[14] PROGMEM       = "I J K L M N O";
const char ABCTextCharacterM[14] PROGMEM       = "J K L M N O P";
const char ABCTextCharacterN[14] PROGMEM       = "K L M N O P Q";
const char ABCTextCharacterO[14] PROGMEM       = "L M N O P Q R";
const char ABCTextCharacterP[14] PROGMEM       = "M N O P Q R S";
const char ABCTextCharacterQ[14] PROGMEM       = "N O P Q R S T";
const char ABCTextCharacterR[14] PROGMEM       = "O P Q R S T U";
const char ABCTextCharacterS[14] PROGMEM       = "P Q R S T U V";
const char ABCTextCharacterT[14] PROGMEM       = "Q R S T U V W";
const char ABCTextCharacterU[14] PROGMEM       = "R S T U V W X";
const char ABCTextCharacterV[14] PROGMEM       = "S T U V W X Y";
const char ABCTextCharacterW[14] PROGMEM       = "T U V W X Y Z";
const char ABCTextCharacterX[14] PROGMEM       = "U V W X Y Z &";
const char ABCTextCharacterY[14] PROGMEM       = "V W X Y Z & '";
const char ABCTextCharacterZ[14] PROGMEM       = "W X Y Z & ' *";
const char ABCTextCharacterSYMBOL1[14] PROGMEM = "X Y Z & ' * +";
const char ABCTextCharacterSYMBOL2[14] PROGMEM = "Y Z & ' * + ,";
const char ABCTextCharacterSYMBOL3[14] PROGMEM = "Z & ' * + , -";
const char ABCTextCharacterSYMBOL4[14] PROGMEM = "& ' * + , - .";
const char ABCTextCharacterSYMBOL5[14] PROGMEM = "' * + , - . /";
const char ABCTextCharacterSYMBOL6[14] PROGMEM = "* + , - . / 0";
const char ABCTextCharacterSYMBOL7[14] PROGMEM = "+ , - . / 0 1";
const char ABCTextCharacterSYMBOL8[14] PROGMEM = ", - . / 0 1 2";
const char ABCTextCharacterNUMBER0[14] PROGMEM = "- . / 0 1 2 3";
const char ABCTextCharacterNUMBER1[14] PROGMEM = ". / 0 1 2 3 4";
const char ABCTextCharacterNUMBER2[14] PROGMEM = "/ 0 1 2 3 4 5";
const char ABCTextCharacterNUMBER3[14] PROGMEM = "0 1 2 3 4 5 6";
const char ABCTextCharacterNUMBER4[14] PROGMEM = "1 2 3 4 5 6 7";
const char ABCTextCharacterNUMBER5[14] PROGMEM = "2 3 4 5 6 7 8";
const char ABCTextCharacterNUMBER6[14] PROGMEM = "3 4 5 6 7 8 9";
const char ABCTextCharacterNUMBER7[12] PROGMEM = "4 5 6 7 8 9";
const char ABCTextCharacterNUMBER8[10] PROGMEM = "5 6 7 8 9";
const char ABCTextCharacterNUMBER9[8] PROGMEM = "6 7 8 9";
const char ABCTextCharacterOPTION1[6] PROGMEM  = "SPACE";
const char ABCTextCharacterOPTION2[5] PROGMEM  = "NEXT";
const char ABCTextCharacterOPTION3[10] PROGMEM = "BACKSPACE";
const char ABCTextCharacterOPTION4[7] PROGMEM  = "CANCEL";
const char ABCTextCharacterOPTION5[5] PROGMEM  = "SAVE";

// Entering letters or numbers:
// The idea is to scroll letters or numbers left or right and show the adjacent character on each side to show that you can choose.
// Example:
// ABC[D]EFG
// Characters: ABCDEFGHIJKLMNOPQRSTUVWXYZ&'*+,-./0123456789 SPACE NEXT BACKSPACE CANCEL SAVE

const char TextGOBACK[8] PROGMEM = "GO BACK";
const char TextNO[3] PROGMEM = "NO";
const char TextSTART[6] PROGMEM = "START";
const char TextCANCEL[7] PROGMEM = "CANCEL";
const char TextCONTINUE[9] PROGMEM = "CONTINUE";
const char TextOK[3] PROGMEM = "OK";
const char TextDELETE[7] PROGMEM = "DELETE";

const char TextTheloadcell[14] PROGMEM = "The load cell";
const char Textrequirescalibration[22] PROGMEM = "requires calibration.";
const char TextDoyouwanttostart[21] PROGMEM = "Do you want to start";
const char Textthefullcalibration[21] PROGMEM = "the full calibration";
const char Textprocess[9] PROGMEM = "process?";

const char TextMAINMENU[10] PROGMEM = "MAIN MENU";
const char TextEDITTHISPROFILE[20] PROGMEM = "- EDIT THIS PROFILE";
const char TextADDNEWPROFILEMenu[18] PROGMEM = "- ADD NEW PROFILE";
const char TextOPTIONSMenu[10] PROGMEM = "- OPTIONS";
const char TextGOBACKMenu[10] PROGMEM = "- GO BACK";

const char TextEDITPROFILE[13] PROGMEM = "EDIT PROFILE";
const char TextTAREZERO[7] PROGMEM = "- TARE";
const char TextEDITPROFILENAMEMenu[20] PROGMEM = "- EDIT PROFILE NAME";
const char TextDELETETHISPROFILE[22] PROGMEM = "- DELETE THIS PROFILE";

const char TextPlaceintheholder[20] PROGMEM = "Place in the holder";
const char Texttheemptyspoolyou[20] PROGMEM = "the empty spool you";
const char Textwanttouseforthis[21] PROGMEM = "want to use for this";
const char Textprofile[9] PROGMEM = "profile.";

const char TextSavingtheweightof[21] PROGMEM = "Saving the weight of";
const char Texttheemptyspool[17] PROGMEM = "the empty spool.";
const char TextPleasedonttouch[20] PROGMEM = "Please, don't touch";
const char Textanything[10] PROGMEM = "anything.";

const char TextTheweightofthe[18] PROGMEM = "The weight of the";
const char Textemptyspoolhasbeen[21] PROGMEM = "empty spool has been";
const char Textsaved[7] PROGMEM = "saved.";

const char TextEDITPROFILENAME[18] PROGMEM = "EDIT PROFILE NAME";
const char TextADDNEWPROFILE[16] PROGMEM = "ADD NEW PROFILE";

const char TextMemoryisfull[16] PROGMEM = "Memory is full.";
const char TextYoucantaddmore[18] PROGMEM = "You can't add any";
const char Textprofiles[15] PROGMEM = "more profiles.";

const char TextTheprofilenamed[18] PROGMEM = "The profile named";
const char Textisgoingtobe[15] PROGMEM = "is going to be";
const char TextdeletedOK[13] PROGMEM = "deleted. OK?";

const char Texthasbeendeleted[18] PROGMEM = "has been deleted.";

const char TextYoucantdelete[17] PROGMEM = "You can't delete";
const char Texttheonlyprofile[17] PROGMEM = "the only profile";
const char Textleft[6] PROGMEM = "left.";

// Options:
const char TextOPTIONS[8] PROGMEM = "OPTIONS";
const char TextDEADZONEMenu[11] PROGMEM = "- DEADZONE";
const char TextFULLCALIBRATION[19] PROGMEM = "- FULL CALIBRATION";
const char TextFACTORYRESETMenu[16] PROGMEM = "- FACTORY RESET";

// Deadzone:
const char TextDEADZONE[9] PROGMEM = "DEADZONE";
const char TextINPUT[6] PROGMEM = "INPUT";
const char TextOUTPUT[7] PROGMEM = "OUTPUT";

const char TextThisprocedureis[18] PROGMEM = "This procedure is";
const char Textabouttakinga[15] PROGMEM = "about taking a";
const char Textreferencefromareal[22] PROGMEM = "reference from a real";
const char Textscaletocalibrate[19] PROGMEM = "scale to calibrate";
const char Texttheloadcell[15] PROGMEM = "the load cell.";

const char TextPlaceanyfullspool[21] PROGMEM = "Place any full spool";
const char Textonarealscaleand[20] PROGMEM = "on a real scale and";
const char Texttakenoteofthe[17] PROGMEM = "take note of the";
const char Textexactweightgrams[22] PROGMEM = "exact weight (grams).";

const char TextEntertheweight[17] PROGMEM = "Enter the weight";

const char TextPlacethatsamefull[21] PROGMEM = "Place that same full";
const char Textspoolintheholder[21] PROGMEM = "spool in the holder.";

const char Textthefullspool[16] PROGMEM = "the full spool.";

const char TextWeightsaved[14] PROGMEM = "Weight saved.";
const char TextRemovethespoolfrom[22] PROGMEM = "Remove the spool from";
const char Texttheholderleaving[20] PROGMEM = "the holder, leaving";
const char Texttheholdercompletely[22] PROGMEM = "the holder completely";
const char Textempty[7] PROGMEM = "empty.";

const char TextThecalibrationhas[20] PROGMEM = "The calibration has";
const char Textbeencompleted[16] PROGMEM = "been completed.";

const char Textg[2] PROGMEM = "g";

const char TextSavingthevalueof[20] PROGMEM = "Saving the value of";
const char Textanemptyholder[17] PROGMEM = "an empty holder.";

const char TextFactoryreseterases[21] PROGMEM = "Factory reset erases";
const char Textallyouruserdata[20] PROGMEM = "all your user data.";
const char TextFactoryresetin[17] PROGMEM = "Factory reset in";
const char Textprogress[10] PROGMEM = "progress.";
const char TextPleasewait[15] PROGMEM = "Please wait...";

// Full Calibration Variables to temporally save the values before going to the EEPROM:
long TemporalFullSpoolRawValue;  // Read EEPROM Full Calibration raw value when holder is full
long TemporalFullSpoolWeight = 1250;  // When doing a full calibration, this is the weight of the full spool selected by the user

int EditTextCursorPosition;  // Editing text - Cursor position

int TextCursorPositionX;  // X position for text cursor

byte BackspacePosition;  // When editing text, the BACKSPACE moves the characters that are on the right side and mvoes it to the left.
                         // This variable is used to temporally store the position of the "while loop" to move all characters.

byte AmountEEPROMAmountSectorsUsed;  // Shows the amount of sectors EEPROM used for profiles


// General EEPROM address:
const int EEPROM_AddressCalibrationDone = 0;  // EEPROM address if Full Calibration was done
                                              // 0 = Full Calibration never doned
                                              // 1 = Full Calibration has been done before
                                              // Uses 1 byte

const byte EEPROM_AddressEmptyHolder = 1;  // EEPROM address for Full Clalibration when holder is empty
                                           // Uses 4 bytes
const byte EEPROM_AddressFullSpool = 5;  // EEPROM address for Full Clalibration when holder has a full spool, raw value
                                         // Uses 4 bytes
const byte EEPROM_AddressFullSpoolWeight = 9;  // EEPROM address for Full Clalibration when holder has a full spool, set weight
                                               // Uses 4 bytes
const byte EEPROM_AddressDeadzoneAmount = 13;  // EEPROM address for the amount of deadzone for the shown weight
                                               // Uses 1 bytes
const byte EEPROM_AddressCurrentProfileSlot = 14;  // EEPROM address to go to the last profile that was active.
                                               // This is so every power cycle, it loads the profile that was open last. 
                                               // Uses 1 bytes
                                               // Next Address available is 15 to 19

// Profile EEPROM address:
// EEPROM Address for profile slot. It uses 30 addresses.
// Each profile byte is an array because each part of the array is a different profile slot
const int EEPROM_AddressEmptySpoolMeasuredWeightProfile[] = {20,50,80,110,140,170,200,230,260,290,320,350,380,410,440,470,500,530,560,590};  // EEPROM address for profile Tare when holder has an empty spool, measured weight
                                                         // Uses 2 bytes
                                                         // It's an array because it's for each profile slot
                                                         // Profile slot goes from 0 to 19

// Characters for profile slot. It uses 22 bytes:
const int EEPROM_AddressName0Profile[] =              {22,52,82,112,142,172,202,232,262,292,322,352,382,412,442,472,502,532,562,592};  // EEPROM address for profile name, character 0
const int EEPROM_AddressName1Profile[] =              {23,53,83,113,143,173,203,233,263,293,323,353,383,413,443,473,503,533,563,593};  // EEPROM address for profile name, character 1
const int EEPROM_AddressName2Profile[] =              {24,54,84,114,144,174,204,234,264,294,324,354,384,414,444,474,504,534,564,594};  // EEPROM address for profile name, character 2
const int EEPROM_AddressName3Profile[] =              {25,55,85,115,145,175,205,235,265,295,325,355,385,415,445,475,505,535,565,595};  // EEPROM address for profile name, character 3
const int EEPROM_AddressName4Profile[] =              {26,56,86,116,146,176,206,236,266,296,326,356,386,416,446,476,506,536,566,596};  // EEPROM address for profile name, character 4
const int EEPROM_AddressName5Profile[] =              {27,57,87,117,147,177,207,237,267,297,327,357,387,417,447,477,507,537,567,597};  // EEPROM address for profile name, character 5
const int EEPROM_AddressName6Profile[] =              {28,58,88,118,148,178,208,238,268,298,328,358,388,418,448,478,508,538,568,598};  // EEPROM address for profile name, character 6
const int EEPROM_AddressName7Profile[] =              {29,59,89,119,149,179,209,239,269,299,329,359,389,419,449,479,509,539,569,599};  // EEPROM address for profile name, character 7
const int EEPROM_AddressName8Profile[] =              {30,60,90,120,150,180,210,240,270,300,330,360,390,420,450,480,510,540,570,600};  // EEPROM address for profile name, character 8
const int EEPROM_AddressName9Profile[] =              {31,61,91,121,151,181,211,241,271,301,331,361,391,421,451,481,511,541,571,601};  // EEPROM address for profile name, character 9
const int EEPROM_AddressName10Profile[] =             {32,62,92,122,152,182,212,242,272,302,332,362,392,422,452,482,512,542,572,602};  // EEPROM address for profile name, character 10
const int EEPROM_AddressName11Profile[] =             {33,63,93,123,153,183,213,243,273,303,333,363,393,423,453,483,513,543,573,603};  // EEPROM address for profile name, character 11
const int EEPROM_AddressName12Profile[] =             {34,64,94,124,154,184,214,244,274,304,334,364,394,424,454,484,514,544,574,604};  // EEPROM address for profile name, character 12
const int EEPROM_AddressName13Profile[] =             {35,65,95,125,155,185,215,245,275,305,335,365,395,425,455,485,515,545,575,605};  // EEPROM address for profile name, character 13
const int EEPROM_AddressName14Profile[] =             {36,66,96,126,156,186,216,246,276,306,336,366,396,426,456,486,516,546,576,606};  // EEPROM address for profile name, character 14
const int EEPROM_AddressName15Profile[] =             {37,67,97,127,157,187,217,247,277,307,337,367,397,427,457,487,517,547,577,607};  // EEPROM address for profile name, character 15
const int EEPROM_AddressName16Profile[] =             {38,68,98,128,158,188,218,248,278,308,338,368,398,428,458,488,518,548,578,608};  // EEPROM address for profile name, character 16
const int EEPROM_AddressName17Profile[] =             {39,69,99,129,159,189,219,249,279,309,339,369,399,429,459,489,519,549,579,609};  // EEPROM address for profile name, character 17
const int EEPROM_AddressName18Profile[] =             {40,70,100,130,160,190,220,250,280,310,340,370,400,430,460,490,520,550,580,610};  // EEPROM address for profile name, character 18
const int EEPROM_AddressName19Profile[] =             {41,71,101,131,161,191,221,251,281,311,341,371,401,431,461,491,521,551,581,611};  // EEPROM address for profile name, character 19
const int EEPROM_AddressName20Profile[] =             {42,72,102,132,162,192,222,252,282,312,342,372,402,432,462,492,522,552,582,612};  // EEPROM address for profile name, character 20
const int EEPROM_AddressName21Profile[] =             {43,73,103,133,163,193,223,253,283,313,343,373,403,433,463,493,523,553,583,613};  // EEPROM address for profile name, character 21

const int EEPROM_AddressAmountOfCharTittleProfile[] = {44,74,104,134,164,194,224,254,284,314,344,374,404,434,464,494,524,554,584,614};  // EEPROM address for the amount of characters for profile name
                                                           // Uses 1 byte
const int EEPROM_AddressActiveOrDisabledProfile[] =   {45,75,105,135,165,195,225,255,285,315,345,375,405,435,465,495,525,555,585,615};  // EEPROM address to know if this profile is in use or not
                                                         // 0 = Profile is empty so is going to be ignored in the main screen
                                                         // 1 = Profile has data recorded so is going to be considered to show
                                                         // Uses 1 byte


// I need to convert UserProfileSlotSequence[] array into an EEPROM array.
// The following is the EEPROM address for that:
const int EEPROM_UserProfileSlotOrderSequence[20] = {700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719};  // This is an array to store the EEPROM memory address into a sequence for the user so it can navigate the profiles in the correct order
                                // Each one saves what address to use in the EEPROM.
                                // By default I have it to correspond to the EEPROM channels, but when people add new profiles, the slot of the next one might require to move it forward to make space for the new one.
                                // The reason is that I want the new profile to always be next to the current profile.
                                // If I would use the EEPROM channel as is and the next profile has already data, then the new profile would not be next to the current profile.
// End of EEPROM Address





// The following is a void to store a long variable into the EEPROM.
// This is because the EEPROM normally can only store a byte which limits the values.
// Since we need to store very large values, we need to use 4 bytes:
void writeLongIntoEEPROM(int address, long number)
{ 
  EEPROM.write(address, (number >> 24) & 0xFF);
  EEPROM.write(address + 1, (number >> 16) & 0xFF);
  EEPROM.write(address + 2, (number >> 8) & 0xFF);
  EEPROM.write(address + 3, number & 0xFF);
}
// Now the same process but now to read long EEPROM:
long readLongFromEEPROM(int address)
{
  return ((long)EEPROM.read(address) << 24) +
         ((long)EEPROM.read(address + 1) << 16) +
         ((long)EEPROM.read(address + 2) << 8) +
         (long)EEPROM.read(address + 3);
}


// The following is a void to store a int variable into the EEPROM.
// This is because the EEPROM normally can only store a byte which limits the values.
// Since we need to store larger values, we need to use 2 bytes:
void writeIntIntoEEPROM(int address, int number)
{ 
  byte byte1 = number >> 8;
  byte byte2 = number & 0xFF;
  EEPROM.write(address, byte1);
  EEPROM.write(address + 1, byte2);
}
// Now the same process but now to read int EEPROM:
int readIntFromEEPROM(int address)
{
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}


// Void to be able to reset arduino:
void(* resetFunc) (void) = 0;//declare reset function at address 0


void ResetSmoothing()  // Start of reseting smoothing and deadzone variables to start calculating from scratch:
{ 
  // Match the correct Profile number to the Profile slot:
  CurrentProfileEEPROMSector = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[CurrentProfileUserSlot]);

  EmptySpoolMeasuredWeight = readIntFromEEPROM(EEPROM_AddressEmptySpoolMeasuredWeightProfile[CurrentProfileEEPROMSector]);  // Read EEPROM Profile Tare measured weight when spool is empty

  // To prevent the smoothing process from starting from 0, we set the smoothing variables to store the current cell values:

  // Read scale sensor:
  RawLoadCellReading = scale.read();

  RawLoadCellReading = RawLoadCellReading /10;  // Remove one digit from the raw reading to reduce the value and could be handle by the map function

  // Remap to convert Raw Values into grams:
  HolderWeight = map (RawLoadCellReading, EmptyHolderRawValue, FullSpoolRawValue, 0, FullSpoolWeight);  // Remaps temperature IN to color value
  HolderWeight = constrain(HolderWeight, 0, 9999);  // limits value between min and max to prevent going over limits
    
  HolderWeight = HolderWeight - EmptySpoolMeasuredWeight;  // Remove the weight of the empty spool

  // Prevent going under 0:
  if(HolderWeight < 0)
  {
    HolderWeight = 0;
  }
  
  // We are going to write all the arrays one by one, until we write all of them.
  // To do that we use a while to create a mini loop that will only finish when condition is false:
  readIndex1 = 0;  // Reset array number
  while(readIndex1 < numReadings1)
  {
    readings1[readIndex1] = HolderWeight;  // Records the current value so the smoothing doesn't start from 0
    readIndex1 = readIndex1 + 1;  // Increase array number to write the following array
  }  // End of while

  total1 = HolderWeight * numReadings1;  // Multiply the current value with the amount of samples/arrays
                                                       // so the smoothing doesn't start from 0.
  readIndex1 = 0;  // Reset array number
  
  // I want the final value to start at the correct value considering the deadzone and to be
  // at the lower edge of the deadzone so I do the following:
  WeightWithDeadzone = HolderWeight;  // Record on deadzone the current value so it doesn't start from 0
  SmoothedWeight = HolderWeight;

  WeightWithDeadzone = WeightWithDeadzone + DeadzoneAmountSet;  // When i boot-up arduino, I want the output to show the
                                        // lower edge of the deazone so the value is close to what it would be in normal operation.
                                        // I manually used to do that every time I boot-up arduino by pressing more weight into
                                        // the holder and it would stabilize in the edge of the dead zone.
                                        // To do this automaticly I added this line in the setup.
                                        // Basically the first value is going to be the current real value of the scale.

}  // End of reseting smoothing and deadzone variables to start calculating from scratch










void setup()  // Start of setup
{
  //Serial.begin(9600);  // Start serial comunication to print debugging values to the serial monitor

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);  // Start scale object and asing the pins

  // Set pins as inputs or outputs:
  pinMode(LeftButtonPin, INPUT_PULLUP);  // Set pin for LEFT button to input and activate pullup resistor
  pinMode(EnterButtonPin, INPUT_PULLUP);  // Set pin for Enter button to input and activate pullup resistor
  pinMode(RightButtonPin, INPUT_PULLUP);  // Set pin for RIGHT button to input and activate pullup resistor

  
  // Read EEPROM:
  
  VariableCalibrationDone = EEPROM.read(EEPROM_AddressCalibrationDone);  // Read EEPROM if Full Calibration has been done
  
  EmptyHolderRawValue = readLongFromEEPROM(EEPROM_AddressEmptyHolder);  // Read EEPROM Full Calibration raw value when holder is empty
  FullSpoolRawValue = readLongFromEEPROM(EEPROM_AddressFullSpool);  // Read EEPROM Full Calibration raw value when holder is full
  FullSpoolWeight = readLongFromEEPROM(EEPROM_AddressFullSpoolWeight);  // Read EEPROM Full Calibration raw value when holder is full


  // Read EEPROM for the amount of deadzone for the shown weight:
  DeadzoneAmountSet = EEPROM.read(EEPROM_AddressDeadzoneAmount);  // Read EEPROM for the amount of deadzone for the shown weight

  
  // Check if the EEPROM value is valid. If not, it means it's in the default so we need to set
  // a valid default value. This is mostly when using a new arduino or if we did a factory reset
  if(DeadzoneAmountSet > 50)  // if the EEPROM that stores the deadzone set amount is above the limit
  {
    EEPROM.write(EEPROM_AddressDeadzoneAmount, 0);  // Write to the EEPROM the default deadzone
    DeadzoneAmountSet = 1;  // Store in the variable the deadzone set amount
  }


  // Check ff the EEPROM for user profile order sequence in slot 0 is over the limit
  // to see if it's valid or not. If it's not valid, it should write the default values:
  if(EEPROM.read(EEPROM_UserProfileSlotOrderSequence[0]) > 19)  // If the EEPROM for user profile order sequence in slot 0 is over the limit
  {
    // We need to write the EEPROM as the default sequence:
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[0], 0);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[1], 1);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[2], 2);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[3], 3);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[4], 4);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[5], 5);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[6], 6);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[7], 7);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[8], 8);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[9], 9);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[10], 10);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[11], 11);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[12], 12);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[13], 13);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[14], 14);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[15], 15);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[16], 16);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[17], 17);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[18], 18);  // Save in the EEPROM the user profile order sequence for that slot
    EEPROM.write(EEPROM_UserProfileSlotOrderSequence[19], 19);  // Save in the EEPROM the user profile order sequence for that slot
  }


  CurrentProfileUserSlot = EEPROM.read(EEPROM_AddressCurrentProfileSlot);  // Read EEPROM for the current user profile slot

  // If the profile slot number is invalid, it means there is no profile saved so go to default values:
  if(CurrentProfileUserSlot > 19)
  {
    EEPROM.write(EEPROM_AddressCurrentProfileSlot, 0);  // Save in the EEPROM the current profile amount
    CurrentProfileUserSlot = 0;

    // Match the correct Profile number to the Profile slot:
    CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];
    
    EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 1);  // Save in the EEPROM the current profile 0 is active
  }


  ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch


  // If Full Calibration has been done:
  // Change the initial page depending on if we ever did the full calibration:
  if(VariableCalibrationDone == 1)  // If we have done the calibration before
  {
    ScreenPage = 100;  // Put page in normal
  }
  // If the value is any other value other than 1, it means we never had done the
  // full calibration so it will stay in ScreenPage 0 (welcome screen)

}  // End of setup










void loop()  // Start of loop
{

  // Read EEPROM:
  EmptyHolderRawValue = readLongFromEEPROM(EEPROM_AddressEmptyHolder);  // Read EEPROM Full Calibration raw value when holder is empty
  FullSpoolRawValue = readLongFromEEPROM(EEPROM_AddressFullSpool);  // Read EEPROM Full Calibration raw value when holder is full
  FullSpoolWeight = readLongFromEEPROM(EEPROM_AddressFullSpoolWeight);  // Read EEPROM Full Calibration raw value when holder is full
  DeadzoneAmountSet = EEPROM.read(EEPROM_AddressDeadzoneAmount);  // Read EEPROM for the amount of deadzone for the shown weight
  CurrentProfileUserSlot = EEPROM.read(EEPROM_AddressCurrentProfileSlot);  // Read EEPROM for the current profile slot number


  // Read EEPROM for the user profile slot order sequence and put it in the variable:
  UserProfileSlotSequence[0] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[0]);
  UserProfileSlotSequence[1] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[1]);
  UserProfileSlotSequence[2] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[2]);
  UserProfileSlotSequence[3] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[3]);
  UserProfileSlotSequence[4] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[4]);
  UserProfileSlotSequence[5] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[5]);
  UserProfileSlotSequence[6] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[6]);
  UserProfileSlotSequence[7] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[7]);
  UserProfileSlotSequence[8] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[8]);
  UserProfileSlotSequence[9] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[9]);
  UserProfileSlotSequence[10] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[10]);
  UserProfileSlotSequence[11] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[11]);
  UserProfileSlotSequence[12] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[12]);
  UserProfileSlotSequence[13] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[13]);
  UserProfileSlotSequence[14] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[14]);
  UserProfileSlotSequence[15] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[15]);
  UserProfileSlotSequence[16] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[16]);
  UserProfileSlotSequence[17] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[17]);
  UserProfileSlotSequence[18] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[18]);
  UserProfileSlotSequence[19] = EEPROM.read(EEPROM_UserProfileSlotOrderSequence[19]);


  // Match the correct Profile number to the Profile slot:
  CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];
    
  // Read EEPROM EmptySpoolMeasuredWeight depending on the profile is on:
  EmptySpoolMeasuredWeight = readIntFromEEPROM(EEPROM_AddressEmptySpoolMeasuredWeightProfile[CurrentProfileEEPROMSector]);  // Read EEPROM Profile Tare measured weight when spool is empty

  // Read all EEPROM profile sectors to count how many are in use:
  AmountEEPROMAmountSectorsUsed = 0;  // Reset so we can start counting from scratch
  CounterForWhileEEPROMSectorState = 0;  // Reset counter to start counting from scratch

  while(CounterForWhileEEPROMSectorState < 20)
  {
    if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[CounterForWhileEEPROMSectorState]) == 1)  // If the EEPROM sector is already in use (enabled)
    {
      AmountEEPROMAmountSectorsUsed = AmountEEPROMAmountSectorsUsed + 1;  // Increase counter to count how many sectors are available
    }
    CounterForWhileEEPROMSectorState = CounterForWhileEEPROMSectorState + 1;  // Increase counter so we can later check the same for the next sector
  }





  // Read load cell only if it's available. This is so the loop run faster
  if(scale.is_ready())
  {
    RawLoadCellReading = scale.read();
    RawLoadCellReading = RawLoadCellReading /10;  // Remove one digit from the raw reading to reduce the value and could be handle by the map function
  }
  

  // Remap to convert Raw Values into grams:
  HolderWeight = map (RawLoadCellReading, EmptyHolderRawValue, FullSpoolRawValue, 0, FullSpoolWeight);  // Remaps temperature IN to color value
  HolderWeight = constrain(HolderWeight, 0, 9999);  // limits value between min and max to prevent going over limits

  // Add to the measurement the weight of the empty spool in that profile so we measure the weight of the filament only:
  CurrentProfileWeightShown = HolderWeight - EmptySpoolMeasuredWeight;
  CurrentProfileWeightShown = constrain(CurrentProfileWeightShown, 0, 9999);  // limits value between min and max to prevent going over limits



  // Smooth Weight:
  // Smoothing process:
  // Subtract the last reading:
  total1 = total1 - readings1[readIndex1];
  // Read value:
  readings1[readIndex1] = CurrentProfileWeightShown;
  // Add the reading to the total:
  total1 = total1 + readings1[readIndex1];
  // Advance to the next position in the array:
  readIndex1 = readIndex1 + 1;

  // if we're at the end of the array...
  if (readIndex1 >= numReadings1)
  {
    readIndex1 = 0;    // Wrap around to the beginning
  }

  // Calculate the average:
  SmoothedWeight = total1 / numReadings1;

  // End of smoothing process






  /////////////
  // Buttons //
  /////////////

  // ENTER button:

  // Debouncing for ENTER button:
  if (digitalRead(EnterButtonPin) == HIGH)  // If button is release, do the following:
  {
    if (ButtonEnterDebouncingCounter == ButtonDebouncingAmount)  // If wait long enough to see if button was really release
    {
      ButtonEnterDebouncingLastState = 0;  // Record that it was release
      EnterButtonActivationStateWithDebounce = 0;  // Record that the debouncing say that we release the button
    }
    else  // If we didn't wait long enough to see if button was really release:
    {
      ButtonEnterDebouncingCounter++;  // Increase counter to keep waiting if button was really release
    }
  }
  else  // If button is press, do the following:
  {
    ButtonEnterDebouncingCounter = 0;  // Reset debouncing counter
    if (ButtonEnterDebouncingLastState == 0)  // If button was released
    {
      ButtonEnterDebouncingLastState = 1;  // Record that button is press
    }
    if (ButtonEnterDebouncingCounter < ButtonDebouncingAmount)  // If is not yet at the limit...
    {
      ButtonEnterDebouncingCounter++;  // Increase counter to keep waiting
    }
    EnterButtonActivationStateWithDebounce = 1;  // Record that debouncing say that we are still pressing the button
  }
  // End of ENTER button debouncing
  

  // Do ENTER button action:
  if(EnterButtonActivationStateWithDebounce == 1)  // If debouncing state of the button say we are pressing the button:
  {
    // Put whatever you want to do when we press the button:
      
    //FlashingCounter = 0;  // Reset flashing counter to stop the flashing
    //FlashState = 1;  // Set flasher state as ON to prevent from flashing while holding the ENTER button
    //framecount2 = 0;  // Reset counter to prevent from going to normal mode automaticly after some time has passed
    EnterButtonHoldRepeatCounter++;  // Increase counter to know for how long we are pressing the button


    // Do only one pulse for the ENTER button:
    if(EnterButtonHoldCounter < 1)  // If the button was not excecuted already:
    {
      // Put whatever you want to do while button is press:
      EnterButtonActivationStatePulses = 1;  // Write state of the ENTER button as active
      EnterButton1PulseOnly = 1;
    }
    else  // If the button was excecuted already:
    {
      EnterButtonActivationStatePulses = 0;  // Write state of the ENTER button as inactive
      EnterButton1PulseOnly = 0;  // Store that we already go through a pulse, so now we are in off position
    }
    EnterButtonHoldCounter = 1;  // Record that we already excecute the action for one pulse, to prevent from excecuting again


    // Repeat when long-holding button:
    if(EnterButtonHoldRepeatCounter > RepeatWhenWeHoldAmount)  // If we hold the button for long enough:
    {
      EnterButtonHoldRepeatCounter = RepeatWhenWeHoldAmount + 1;  // Prevents the counter from going indefinitely up
      // Put whatever you want to do when we hold the button for long enough:

      EnterButtonActivationStateLongHold = 1;  // Record that we are long-holding the button
      
      // Create repeat pulses:
      if(EnterRepeatWhenWeHoldFrequencyCounter == 0)  // If repeat pulse is not counting
      {
        // Put whatever you want to do when we hold the button and a repeat pulse was created:
        EnterButtonActivationStatePulses = 1;  // Excecute button action
      }
      if(EnterRepeatWhenWeHoldFrequencyCounter > RepeatWhenWeHoldFrequencyDelay)  // If we reach limit for the counter for the pulses delay
      {
        EnterRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay to restart cycle of waiting
      }
      else  // If we didn't wait long enough:
      {
        EnterRepeatWhenWeHoldFrequencyCounter++;  // Increase counter to keep waiting
      }
    }
    
    // End of hold enter button:
  
  }  // End of if debouncing state of the button say we are pressing the button
  else  // If debouncing state of the button say we release the button:
  {
    // Put whatever you want to do when we release the button:

    EnterButtonHoldCounter = 0;  // Record that we release the button
    EnterButtonHoldRepeatCounter = 0;  // Reset the counter that counts for how long we are pressing the button
    EnterButtonActivationStatePulses = 0;  // Write state of the button as inactive
    EnterButtonRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay for the repeat when holding
    EnterButtonActivationStateLongHold = 0;  // Record that we are not longer long-holding the button
    EnterButton1PulseOnly = 0;  // Reset the one pulse only as release
  }
  // End of if we release Enter button






  // RIGHT button:

  // Debouncing for RIGHT button:
  if (digitalRead(RightButtonPin) == HIGH)  // If button is release, do the following:
  {
    if (ButtonRightDebouncingCounter == ButtonDebouncingAmount)  // If wait long enough to see if button was really release
    {
      ButtonRightDebouncingLastState = 0;  // Record that it was release
      RightButtonActivationStateWithDebounce = 0;  // Record that the debouncing say that we release the button
    }
    else  // If we didn't wait long enough to see if button was really release:
    {
      ButtonRightDebouncingCounter++;  // Increase counter to keep waiting if button was really release
    }
  }
  else  // If button is press, do the following:
  {
    ButtonRightDebouncingCounter = 0;  // Reset debouncing counter
    if (ButtonRightDebouncingLastState == 0)  // If button was released
    {
      ButtonRightDebouncingLastState = 1;  // Record that button is press
    }
    if (ButtonRightDebouncingCounter < ButtonDebouncingAmount)  // If is not yet at the limit...
    {
      ButtonRightDebouncingCounter++;  // Increase counter to keep waiting
    }
    RightButtonActivationStateWithDebounce = 1;  // Record that debouncing say that we are still pressing the button
  }
  // End of RIGHT button debouncing
  

  // Do RIGHT button action:
  if(RightButtonActivationStateWithDebounce == 1)  // If debouncing state of the button say we are pressing the button:
  {
    // Put whatever you want to do when we press the button:
      
    //FlashingCounter = 0;  // Reset flashing counter to stop the flashing
    //FlashState = 1;  // Set flasher state as ON to prevent from flashing while holding the Right button
    //framecount2 = 0;  // Reset counter to prevent from going to normal mode automaticly after some time has passed
    RightButtonHoldRepeatCounter++;  // Increase counter to know for how long we are pressing the button

    // Do only one pulse for the right button:
    if(RightButtonHoldCounter < 1)  // If the button was not excecuted already:
    {
      // Put whatever you want to do while button is press:
      RightButtonActivationStatePulses = 1;  // Write state of the Right button as active
      RightButton1PulseOnly = 1;
    }
    else  // If the button was excecuted already:
    {
      RightButtonActivationStatePulses = 0;  // Write state of the Right button as inactive
      RightButton1PulseOnly = 0;  // Store that we already go through a pulse, so now we are in off position
    }
    RightButtonHoldCounter = 1;  // Record that we already excecute the action for one pulse, to prevent from excecuting again


    // Repeat when long-holding button:
    if(RightButtonHoldRepeatCounter > RepeatWhenWeHoldAmount)  // If we hold the button for long enough:
    {
      RightButtonHoldRepeatCounter = RepeatWhenWeHoldAmount + 1;  // Prevents the counter from going indefinitely up
      // Put whatever you want to do when we hold the button for long enough:

      RightButtonActivationStateLongHold = 1;  // Record that we are long-holding the button

      
      // Create repeat pulses:
      if(RightButtonRepeatWhenWeHoldFrequencyCounter == 0)  // If repeat pulse is not counting
      {
        // Put whatever you want to do when we hold the button and a repeat pulse was created:
      
      }
      if(RightButtonRepeatWhenWeHoldFrequencyCounter > RepeatWhenWeHoldFrequencyDelay)  // If we reach limit for the counter for the pulses delay
      {
        RightButtonActivationStatePulses = 1;  // Excecute button action
        RightButtonRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay to restart cycle of waiting
      }
      else  // If we didn't wait long enough:
      {
        RightButtonRepeatWhenWeHoldFrequencyCounter++;  // Increase counter to keep waiting
      }
    }
  }  // End of if debouncing state of the button say we are pressing the button
  else  // If debouncing state of the button say we release the button:
  {
    // Put whatever you want to do when we release the button:

    RightButtonHoldCounter = 0;  // Record that we release the button
    RightButtonHoldRepeatCounter = 0;  // Reset the counter that counts for how long we are pressing the button
    RightButtonActivationStatePulses = 0;  // Write state of the button as inactive
    RightButtonRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay for the repeat when holding
    RightButtonActivationStateLongHold = 0;  // Record that we are not longer long-holding the button
    RightButton1PulseOnly = 0;  // Reset the one pulse only as release
  }
  // End of if we release RIGHT button





  // LEFT button:

  // Debouncing for LEFT button:
  if (digitalRead(LeftButtonPin) == HIGH)  // If button is release, do the following:
  {
    if (ButtonLeftDebouncingCounter == ButtonDebouncingAmount)  // If wait long enough to see if button was really release
    {
      ButtonLeftDebouncingLastState = 0;  // Record that it was release
      LeftButtonActivationStateWithDebounce = 0;  // Record that the debouncing say that we release the button
    }
    else  // If we didn't wait long enough to see if button was really release:
    {
      ButtonLeftDebouncingCounter++;  // Increase counter to keep waiting if button was really release
    }
  }
  else  // If button is press, do the following:
  {
    ButtonLeftDebouncingCounter = 0;  // Reset debouncing counter
    if (ButtonLeftDebouncingLastState == 0)  // If button was released
    {
      ButtonLeftDebouncingLastState = 1;  // Record that button is press
    }
    if (ButtonLeftDebouncingCounter < ButtonDebouncingAmount)  // If is not yet at the limit...
    {
      ButtonLeftDebouncingCounter++;  // Increase counter to keep waiting
    }
    LeftButtonActivationStateWithDebounce = 1;  // Record that debouncing say that we are still pressing the button
  }
  // End of LEFT button debouncing
  


  // Do LEFT button actions:
  if(LeftButtonActivationStateWithDebounce == 1)  // If debouncing state of the button say we are pressing the button:
  {
    // Put whatever you want to do when we press the button:

    //FlashingCounter = 0;  // Reset flashing counter to stop the flashing
    //FlashState = 1;  // Set flasher state as ON to prevent from flashing while holding the LEFT button
    //framecount2 = 0;  // Reset counter to prevent from going to normal mode automaticly after some time has passed
    LeftButtonHoldRepeatCounter++;  // Increase counter to know for how long we are pressing the button

    // Do only one pulse for the button:
    if(LeftButtonHoldCounter < 1)  // If the button was not excecuted already:
    {
      // Put whatever you want to do while button is press:
      LeftButtonActivationStatePulses = 1;  // Write state of the LEFT button as active
      LeftButton1PulseOnly = 1;
    }
    else  // If the button was excecuted already:
    {
      LeftButtonActivationStatePulses = 0;  // Write state of the LEFT button as inactive
      LeftButton1PulseOnly = 0;  // Store that we already go through a pulse, so now we are in off position
    }
    LeftButtonHoldCounter = 1;  // Record that we already excecute the action for one pulse, to prevent from excecuting again


    // Repeat when long-holding button:
    if(LeftButtonHoldRepeatCounter > RepeatWhenWeHoldAmount)  // If we hold the button for long enough:
    {
      LeftButtonHoldRepeatCounter = RepeatWhenWeHoldAmount + 1;  // Prevents the counter from going indefinitely up
      // Put whatever you want to do when we hold the button for long enough:

      LeftButtonActivationStateLongHold = 1;  // Record that we are long-holding the button
      
      // Create repeat pulses:
      if(LeftRepeatWhenWeHoldFrequencyCounter == 0)  // If repeat pulse is not counting
      {
        // Put whatever you want to do when we hold the button and a repeat pulse was created:
        LeftButtonActivationStatePulses = 1;  // Excecute button action
      }
      if(LeftRepeatWhenWeHoldFrequencyCounter > RepeatWhenWeHoldFrequencyDelay)  // If we reach limit for the counter for the pulses delay
      {
        LeftRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay to restart cycle of waiting
      }
      else  // If we didn't wait long enough:
      {
        LeftRepeatWhenWeHoldFrequencyCounter++;  // Increase counter to keep waiting
      }
    }
  }  // End of if debouncing state of the button say we are pressing the button
  else  // If debouncing state of the button say we release the button:
  {
    // Put whatever you want to do when we release the button:

    LeftButtonHoldCounter = 0;  // Record that we release the button
    LeftButtonHoldRepeatCounter = 0;  // Reset the counter that counts for how long we are pressing the button
    LeftButtonActivationStatePulses = 0;  // Write state of the button as inactive
    LeftRepeatWhenWeHoldFrequencyCounter = 0;  // Reset counter that count the pulse delay for the repeat when holding
    LeftButtonActivationStateLongHold = 0;  // Record that we are not longer long-holding the button
    LeftButton1PulseOnly = 0;  // Reset the one pulse only as release
  }
  // End of if we release LEFT button










  //////////////////////////////////////////
  // Button Actions depending on the page //
  //////////////////////////////////////////

  //*******Page index:
  // 0    = Welcome screen recommending starting full calibration
  // 100  = Normal screen showing profile name and weight
  // 1001 = Main menu
  // 2101 = Main menu > Edit profile menu
  // 2111 = Main menu > Edit profile > Tare 1: Place in the holder the empty spool
  // 2120 = Main menu > Edit profile > Tare 2: Saving the weight
  // 2131 = Main menu > Edit profile > Tare 3: Weight saved, with selection on OK
  // 2201 = Main menu > Edit profile > Edit name
  // 2301 = Main menu > Edit profile > Delete profile, with selection on CANCEL
  // 2311 = Main menu > Edit profile > Profile has been deleted, with selection on OK
  // 2312 = Main menu > Edit profile > Delete profile > Only 1 profile left so can't delete
  // 3001 = Main menu > New profile
  // 3012 = Main menu > New profile > No more space available
  // 3201 = Main menu > Options
  // 3210 = Main menu > Options > Deadzone
  // 4001 = Main menu > Options > Full calibration intro, with option on CONTINUE
  // 4011 = Main menu > Options > Full calibration > 2. Place full spool in real scale, with option on CONTINUE
  // 4021 = Main menu > Options > Full calibration > 3. Enter the weight, with option on SAVE
  // 4031 = Main menu > Options > Full calibration > 4. Put full spool on holder, with option on CONTINUE
  // 4040 = Main menu > Options > Full calibration > 5. Saving the weight of the full spool
  // 4051 = Main menu > Options > Full calibration > 6. Remove spool, with selection on CONTINUE
  // 4060 = Main menu > Options > Full calibration > 7. Saving weight of empty holder
  // 4071 = Main menu > Options > Full calibration > 8. Calibration has been completed, with selection on OK
  // 3310 = Main menu > Options > FACTORY RESET
  // 3311 = Main menu > Options > FACTORY RESET 2


  if(ScreenPage == 0)  // If we are in page 0
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in START
      {
        ScreenPage = 4001;  // Go to page full calibration
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      else if(SelectorPosition == 1)  // If selector is in NO
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }
  }  // End of if we are in page 0



  

  if(ScreenPage == 100)  // If we are in page normal
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left buttons is press
    {
     
      // Check if there are more than 1 profile recorded:
      if(AmountEEPROMAmountSectorsUsed > 1)  // If there are more than 1 profile recorded
      {
        CheckPreviousProfile:  // This is where to come back if the previous profile slot is not active

        // Go to previous profile:
        if(CurrentProfileUserSlot > 0)
        {
          CurrentProfileUserSlot = CurrentProfileUserSlot -1;
        }
        else
        {
          CurrentProfileUserSlot = 19;
        }

        // Match the correct Profile number to the Profile slot:
        CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];
    
        // Check if the previous profile slot is active:
        if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector]) != 1)  // If the current profile slot is not active
        {
          goto CheckPreviousProfile;  // Return to check the previous profile slot to see if it's active
        }
        // If the current profile slot is active, is going to continue the code from here
      }  // End of if there are more than 1 profile recorded

      EEPROM.write(EEPROM_AddressCurrentProfileSlot, CurrentProfileUserSlot);  // Save in the EEPROM the profile 1 name

      ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch

      goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
    }  // End of if left buttons is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Check if there are more than 1 profile recorded:
      if(AmountEEPROMAmountSectorsUsed > 1)  // If there are more than 1 profile recorded
      {
        CheckNextProfile:  // This is where to come back if the next profile slot is not active
      
        // Go to next profile:
        if(CurrentProfileUserSlot < 19)
        {
          CurrentProfileUserSlot = CurrentProfileUserSlot +1;
        }
        else
        {
          CurrentProfileUserSlot = 0;
        }

        // Match the correct Profile number to the Profile slot:
        CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];

        // Check if the next profile slot is active:
        if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector]) != 1)  // If the current profile slot is not active
        {
          goto CheckNextProfile;  // Return to check the previous profile slot to see if it's active
        }
        // If the current profile slot is active, is going to continue the code from here
      
        EEPROM.write(EEPROM_AddressCurrentProfileSlot, CurrentProfileUserSlot);  // Save in the EEPROM the current user profile slot

        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }  // End of if there are more than 1 profile recorded
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      ScreenPage = 1001;  // Go to page main menu
      SelectorPosition = 0;  // Reset selector position to 0
      goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
    }
  }  // End of if we are in page normal





  if(ScreenPage == 1001)  // If we are in page main menu
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // Move selector backwards:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 3)// If selector is in value 3
      {
        SelectorPosition = 2;  // Put selector in value 2
      }
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Move selector forward:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 2;  // Put selector in value 2
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 3)// If selector is in value 3
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in EDIT THIS PROFILE
      {
        ScreenPage = 2101;  // Go to page EDIT THIS PROFILE
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      
      if(SelectorPosition == 1)  // If selector is in ADD NEW PROFILE
      {
        // Check if there is profile sectors available:
        if(AmountEEPROMAmountSectorsUsed < 20)  // If there are profile sectors available
        {
          // Profile user slot:
          // Check the next profile user slot available.
          // If the next slot is already in use, we should move it forward to make space

          TemporalCurrentProfileUserSlot = CurrentProfileUserSlot +1;  // Add 1 to the current slot to check next slot

          // If reach the limit, come back to 0:
          if(TemporalCurrentProfileUserSlot > 19)
          {
            TemporalCurrentProfileUserSlot = 0;  // Return to 0
          }

          TargetUserProfileSlot = TemporalCurrentProfileUserSlot;  // Store the next slot from the current one as a target where we are going to get available


          // The next while loop is going to run until the next user profile slot is available.
          // This is in case the next slot is already been used, we are going to
          // move them forward to make space for the new profile so it can be next to the
          // current profile when we selected to add a new profile.
          while((EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[UserProfileSlotSequence[TargetUserProfileSlot]])) == 1)  // While the target is not available
          {
            //Serial.print(TargetUserProfileSlot);
            //Serial.print("-");
            //Serial.print((EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[TargetUserProfileSlot])));
            //Serial.print("\t");
            //Serial.print(TemporalCurrentProfileUserSlot);
            //Serial.print("-");
            //Serial.print((EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[UserProfileSlotSequence[TemporalCurrentProfileUserSlot]])));
              
            if((EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[UserProfileSlotSequence[TemporalCurrentProfileUserSlot]])) == 1)  // If the current slot we are checking is not available (1)
            {
              // Increase the temporal slot so later we can keep checking other slots forward:
              TemporalCurrentProfileUserSlot = TemporalCurrentProfileUserSlot + 1;  // Increase the slot we are checking to check the next one

              // Check if we are in the limit so we return to 0
              if(TemporalCurrentProfileUserSlot > 19)
              {
                TemporalCurrentProfileUserSlot = 0;
              }
            }
            else  // If the current temporal profile we are checking is available (0)
            {
              // Save the current slot so we know where to move the previous slot to this one;
              TemporalSlotAvailable = UserProfileSlotSequence[TemporalCurrentProfileUserSlot];  // Save the slot we are right now to the temporal variable se we know the slot that is available

              // Move user temporal slot to previous one:
              TemporalSlotToBeCopy = TemporalCurrentProfileUserSlot - 1;  // Save in the variable the previous profile, that should be in use
              // Make sure stays in the limits:
              if(TemporalSlotToBeCopy > 19)  // If it's out of the limits
              {
                TemporalSlotToBeCopy = 19;  // Return to the maximum correct limit
              }

              //Serial.print("\t");
              //Serial.print(TemporalSlotAvailable);// Profile slot we are going to write with the previous profile. This is used when we copy/move slots when adding a new profile
              //Serial.print("\t");
              //Serial.print(TemporalSlotToBeCopy);  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile
              //Serial.print("\t");
              //Serial.print(TemporalCurrentProfileUserSlot); 
              //Serial.print("-");
              //Serial.print(UserProfileSlotSequence[TemporalCurrentProfileUserSlot]);  // EEPROM sector available where the previous profile is going to move to
              //Serial.print("\t");
              //Serial.print(TemporalSlotToBeCopy);  
              //Serial.print("-");
              //Serial.print(UserProfileSlotSequence[TemporalSlotToBeCopy]);  // EEPROM sector that we are going to move to the available EEPROM sector
              
              // Now that we have the available slot and the slot that we are going to move
              // we can move it:
              // A > B
              SectorThatNowIsAvailable = UserProfileSlotSequence[TemporalCurrentProfileUserSlot];

              UserProfileSlotSequence[TemporalCurrentProfileUserSlot] = UserProfileSlotSequence[TemporalSlotToBeCopy];  // Save in the current slot the EEPROM sector of the previous slot so now we can leave the previous slot available

              //Serial.print("\t");
              //Serial.print(TemporalCurrentProfileUserSlot);  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile
              //Serial.print("-");
              //Serial.print(UserProfileSlotSequence[TemporalCurrentProfileUserSlot]);  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile


              // Now that we moved the previous slot available, we need to set it as the EEPROM sector that we left out (available)
              // A < B
              UserProfileSlotSequence[TemporalSlotToBeCopy] = SectorThatNowIsAvailable;  // The profile slot that we left available, we set it as the one that has a profile sector avaiable
              //Serial.print("\t");
              //Serial.print(TemporalSlotToBeCopy);  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile
              //Serial.print("-");
              //Serial.print(UserProfileSlotSequence[TemporalSlotToBeCopy]);  // Profile slot we are going to copy to the next available slot. This is used when we copy/move slots when adding a new profile


              // Until this point where it used to be A-B and now it is B-A
              // but now we need to go backwards the current slot we are checking so we can do the same but now one slot backwards
              TemporalCurrentProfileUserSlot = TemporalCurrentProfileUserSlot - 1;  // Go one step backwards in the current profile position
              if(TemporalCurrentProfileUserSlot > 19)  // If it's above the limit. This could be because we were in 0 and now it overflow to 255
              {
                TemporalCurrentProfileUserSlot = 19;  // Set to the valid limit
              }
            }  // End of if the current temporal profile we are checking is available

            /*
            Serial.println("");
            Serial.print(UserProfileSlotSequence[0]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[1]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[2]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[3]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[4]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[5]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[6]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[7]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[8]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[9]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[10]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[11]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[12]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[13]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[14]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[15]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[16]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[17]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[18]);
            Serial.print("-");
            Serial.print(UserProfileSlotSequence[19]);
            Serial.println("");

            // The following is to print the if each EEPROM sector is available or not:
            // 1 = In use
            // 0 = Available
            int testToSerial;  // just temporal por serial
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[0]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print("0.");
            Serial.print(testToSerial);
            Serial.print("-1.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[1]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-2.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[2]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-3.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[3]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-4.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[4]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-5.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[5]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-6.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[6]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-7.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[7]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-8.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[8]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-9.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[9]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-10.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[10]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-11.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[11]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-12.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[12]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-13.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[13]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-14.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[14]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-15.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[15]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-16.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[16]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-17.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[17]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-18.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[18]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.print("-19.");
            if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[19]) == 1){testToSerial=1;}else{testToSerial=0;}
            Serial.print(testToSerial);
            Serial.println("");
            */

          }  // It's going to go out of the while loop, it means the slot target is available

          // Write the EEPROM with the new user profile slot order sequence:
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[0], UserProfileSlotSequence[0]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[1], UserProfileSlotSequence[1]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[2], UserProfileSlotSequence[2]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[3], UserProfileSlotSequence[3]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[4], UserProfileSlotSequence[4]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[5], UserProfileSlotSequence[5]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[6], UserProfileSlotSequence[6]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[7], UserProfileSlotSequence[7]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[8], UserProfileSlotSequence[8]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[9], UserProfileSlotSequence[9]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[10], UserProfileSlotSequence[10]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[11], UserProfileSlotSequence[11]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[12], UserProfileSlotSequence[12]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[13], UserProfileSlotSequence[13]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[14], UserProfileSlotSequence[14]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[15], UserProfileSlotSequence[15]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[16], UserProfileSlotSequence[16]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[17], UserProfileSlotSequence[17]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[18], UserProfileSlotSequence[18]);  // Save in the EEPROM the user profile order sequence for that slot
          EEPROM.write(EEPROM_UserProfileSlotOrderSequence[19], UserProfileSlotSequence[19]);  // Save in the EEPROM the user profile order sequence for that slot

          // Match the correct Profile number to the Profile slot:
          CurrentProfileEEPROMSector = UserProfileSlotSequence[TargetUserProfileSlot];
          
          EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 1);  // Record this profile as enabled

          EEPROM.write(EEPROM_AddressCurrentProfileSlot, TargetUserProfileSlot);  // Save in the EEPROM the current user profile slot
          AmountOfCharProfileName = 1;  // Save in the EEPROM an invalid amount of characters so later is going to reset to default name

          // Set temporal profile name:
          ProfileTitleTemporal[0] = ' ';
          ProfileTitleTemporal[1] = ' ';
          ProfileTitleTemporal[2] = ' ';
          ProfileTitleTemporal[3] = ' ';
          ProfileTitleTemporal[4] = ' ';
          ProfileTitleTemporal[5] = ' ';
          ProfileTitleTemporal[6] = ' ';
          ProfileTitleTemporal[7] = ' ';
          ProfileTitleTemporal[8] = ' ';
          ProfileTitleTemporal[9] = ' ';
          ProfileTitleTemporal[10] = ' ';
          ProfileTitleTemporal[11] = ' ';
          ProfileTitleTemporal[12] = ' ';
          ProfileTitleTemporal[13] = ' ';
          ProfileTitleTemporal[14] = ' ';
          ProfileTitleTemporal[15] = ' ';
          ProfileTitleTemporal[16] = ' ';
          ProfileTitleTemporal[17] = ' ';
          ProfileTitleTemporal[18] = ' ';
          ProfileTitleTemporal[19] = ' ';
          ProfileTitleTemporal[20] = ' ';

          // Write the default empty spool weight:
          writeIntIntoEEPROM(EEPROM_AddressEmptySpoolMeasuredWeightProfile[CurrentProfileEEPROMSector], 0);  // Write to EEPROM the measured weight when spool is empty in Profile Tare

          ScreenPage = 3001;  // Go to page New profile
          SelectorPosition = 0;  // Reset selector position to 0
          goto AfterPageSelection;  // Skip the rest part of the code to check for button presses

        }  // End of if there are profile sectors available
        else  // If there are not profile sectors available
        {
          ScreenPage = 3012;  // Go to page "no more space available"
          SelectorPosition = 0;  // Reset selector position to 0
          goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
        }  // End of if there are not profile sectors available
      }  // End of if selector is in ADD NEW PROFILE
      if(SelectorPosition == 2)  // If selector is in OPTIONS
      {
        ScreenPage = 3201;  // Go to page OPTIONS
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 3)  // If selector is in GO BACK
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page main menu



  

  if(ScreenPage == 2101)  // If we are in page EDIT PROFILE MENU
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // Move selector backwards:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 3)// If selector is in value 3
      {
        SelectorPosition = 2;  // Put selector in value 2
      }
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Move selector forward:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 2;  // Put selector in value 2
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 3)// If selector is in value 3
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in Tare
      {
        ScreenPage = 2111;  // Go to page  Tare 1
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in EDIT PROFILE NAME
      {
        ScreenPage = 2201;  // Go to page EDIT PROFILE NAME
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 2)  // If selector is in DELETE THIS PROFILE
      {
        // Check if we have more than 1 profile:
        // This is because if we have only 1 profile, we are not going to allowed.
        if(AmountEEPROMAmountSectorsUsed > 1)  // If there are more than 1 profile recorded
        {
          ScreenPage = 2301;  // Go to page DELETE THIS PROFILE
          SelectorPosition = 0;  // Reset selector position to 0
          goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
        }
        else
        {
          ScreenPage = 2312;  // Go to page only one profile left so you can't delete it
          SelectorPosition = 0;  // Reset selector position to 0
          goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
        }
      }
      if(SelectorPosition == 3)  // If selector is in GO BACK
      {
        ScreenPage = 1001;  // Go to main menu
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page EDIT PROFILE MENU





  if(ScreenPage == 2111)  // If we are in page Tare 1
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CONTINUE
      {
        ScreenPage = 2120;  // Go to page Tare 2
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Tare 1





  if(ScreenPage == 2201 || ScreenPage == 3001)  // If page is on EDIT PROFILE NAME or NEW PROFILE
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // If selector is 0, go to the limit:
      if(SelectorPosition == 0)  // If selector is in minimum limit
      {
        SelectorPosition = 48;  // Put selector in maximum limit
      }
      else  // If selector is not in limit
      {
        SelectorPosition = SelectorPosition -1;  // Reduce selector value by 1
      }
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Move selector forward:
      if(SelectorPosition == 48)  // If selector is maximum limit
      {
        SelectorPosition = 0;  // Put selector in minimum limit to start over
      }
      else  // If selector is not in limit
      {
        SelectorPosition = SelectorPosition + 1;  // Increase selector value
      }
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButtonActivationStatePulses == 1)  // If Enter button is press, inclusing long holding for fast repeat
    //if(EnterButton1PulseOnly == 1)  // If Enter button is press, 1 pulse only (no repeating when long holding)
    {
      // Here's the table of values that correspond to the selection:
      // 0  = A
      // 1  = B
      // 2  = C
      // 3  = D
      // 4  = E
      // 5  = F
      // 6  = G
      // 7  = H
      // 8  = I
      // 9  = J
      // 10 = K
      // 11 = L
      // 12 = M
      // 13 = N
      // 14 = O
      // 15 = P
      // 16 = Q
      // 17 = R
      // 18 = S
      // 19 = T
      // 20 = U
      // 21 = V
      // 22 = W
      // 23 = X
      // 24 = Y
      // 25 = Z
      // 26 SYMBOL1 = &
      // 27 SYMBOL2 = '
      // 28 SYMBOL3 = *
      // 29 SYMBOL4 = +
      // 30 SYMBOL5 = ,
      // 31 SYMBOL6 = -
      // 32 SYMBOL7 = .
      // 33 SYMBOL8 = /
      // 34 NUMBER0 = 0
      // 35 NUMBER1 = 1
      // 36 NUMBER2 = 2
      // 37 NUMBER3 = 3
      // 38 NUMBER4 = 4
      // 39 NUMBER5 = 5
      // 40 NUMBER6 = 6
      // 41 NUMBER7 = 7
      // 42 NUMBER8 = 8
      // 43 NUMBER9 = 9
      // 44 OPTION1 = [SPACE]
      // 45 OPTION2 = [NEXT]
      // 46 OPTION3 = [BACKSPACE]
      // 47 OPTION4 = [CANCEL]
      // 48 OPTION5 = [SAVE]
      
      if(SelectorPosition == 0)  // If selector is in letter A
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'A';
                
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in letter B
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'B';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 2)  // If selector is in letter C
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'C';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 3)  // If selector is in letter D
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'D';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 4)  // If selector is in letter E
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'E';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 5)  // If selector is in letter F
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'F';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 6)  // If selector is in letter G
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'G';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 7)  // If selector is in letter H
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'H';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 8)  // If selector is in letter I
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'I';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 9)  // If selector is in letter J
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'J';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 10)  // If selector is in letter K
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'K';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 11)  // If selector is in letter L
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'L';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 12)  // If selector is in letter M
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'M';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 13)  // If selector is in letter N
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'N';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 14)  // If selector is in letter O
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'O';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 15)  // If selector is in letter P
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'P';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 16)  // If selector is in letter Q
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'Q';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 17)  // If selector is in letter R
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'R';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 18)  // If selector is in letter S
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'S';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 19)  // If selector is in letter T
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'T';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 20)  // If selector is in letter U
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'U';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 21)  // If selector is in letter V
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'V';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 22)  // If selector is in letter W
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'W';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 23)  // If selector is in letter X
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'X';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 24)  // If selector is in letter Y
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'Y';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 25)  // If selector is in letter Z
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 'Z';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      } 
      if(SelectorPosition == 26)  // If selector is in letter &
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '&';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 27)  // If selector is in letter '
      {
        ProfileTitleTemporal[EditTextCursorPosition] = 39;
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 28)  // If selector is in letter *
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '*';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 29)  // If selector is in letter +
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '+';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 30)  // If selector is in letter ,
      {
        ProfileTitleTemporal[EditTextCursorPosition] = ',';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 31)  // If selector is in letter -
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '-';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 32)  // If selector is in letter .
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '.';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 33)  // If selector is in letter /
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '/';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 34)  // If selector is in letter 0
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '0';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 35)  // If selector is in letter 1
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '1';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 36)  // If selector is in letter 2
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '2';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 37)  // If selector is in letter 3
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '3';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 38)  // If selector is in letter 4
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '4';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 39)  // If selector is in letter 5
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '5';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 40)  // If selector is in letter 6
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '6';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 41)  // If selector is in letter 7
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '7';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 42)  // If selector is in letter 8
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '8';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 43)  // If selector is in letter 9
      {
        ProfileTitleTemporal[EditTextCursorPosition] = '9';
        
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }

      
      if(SelectorPosition == 44)  // If selector is in [SPACE]
      {
        ProfileTitleTemporal[EditTextCursorPosition] = ' ';
        // Increase the amount of characters in the profile name
        // if we are at the edge of the set amount. Also, prevent going
        // over limit for amount of characters:
        if(EditTextCursorPosition == AmountOfCharProfileName - 1)  // If the text cursor is at the limit set of amount of characters
        {
          if(AmountOfCharProfileName < 21)  // If the amount of characters is below the maximum limit
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }  // End of if selector is in [SPACE]

      
      if(SelectorPosition == 45)  // If selector is in [NEXT]
      {

        // The following is so if the current character is not a SPACE, pressing NEXT will go forward 1 character:
        if(AmountOfCharProfileName < 21 && ProfileTitleTemporal[EditTextCursorPosition] != ' ')  // If amount of characters is 1
        {
          if(EditTextCursorPosition == AmountOfCharProfileName -1)  // If we are in the end of the name
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Set amount of characters as 2            
          }
       }
                
        // Increase text cursor, but make sure EditTextCursorPosition stays inside limits:
        if(EditTextCursorPosition > AmountOfCharProfileName - 2)  // If text cursor is above limit
        {
          EditTextCursorPosition = 0;  // Reset text cursor to the begining
        }
        else  // If text cursor is inside limits
        {
          EditTextCursorPosition = EditTextCursorPosition + 1;  // Increase cursor
        }

        // If there is only one character and it's not a space
        // I want the NEXT button to go to the 2nd character.
        if(EditTextCursorPosition == 0 && AmountOfCharProfileName == 1)  // If the text cursor is at the begining and there is only 1 character
        {
          if(ProfileTitleTemporal[0] != ' ')  // If the 1st character is not a SPACE
          {
            AmountOfCharProfileName = AmountOfCharProfileName + 1;  // Increase the amount of characters for the name
            ProfileTitleTemporal[EditTextCursorPosition + 1] = ' ';  // Set the next character as SPACE to clear whatever is recorded there
          }
        }  // End of if the text cursor is at the begining and there is only 1 character

        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
     
      }  // End of if selector is in [NEXT]


      if(SelectorPosition == 46)  // If selector is in [BACKSPACE]
      {
        if(EditTextCursorPosition > 0)  // If the text cursor position is 1 or more
        {
          AmountOfCharProfileName = AmountOfCharProfileName - 1;  // Decrease the amount of characters for the name by 1
          EditTextCursorPosition = EditTextCursorPosition - 1;  // Move the text cursor 1 to the left

          // Move the text that is on the right of the cursor, move it to the left:
          // First thing to do is transfer the cursor position to the temporal variable to keep track of the characters:
          BackspacePosition = EditTextCursorPosition;  // Put the cursor position in a temporal variable to know where we start doing the backspace for moving characters

          while(BackspacePosition < 21)  // While the backspace position is below limit, do the following loop:
          {
            ProfileTitleTemporal[BackspacePosition] = ProfileTitleTemporal[BackspacePosition + 1];  // Copy the next character to the temporal current position.
            BackspacePosition = BackspacePosition + 1;  // Add 1 to the backspace temporal position so later in the loop does this in the next character
          }
        }  // End of if the text cursor position is above the minimum

        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }  // End of if selector is in [BACKSPACE]

      
      if(SelectorPosition == 47)  // If selector is in CANCEL
      {
        // If we are creating a new profile, when press CANCEL it should delete the profile to cancel operation:
        if(ScreenPage == 3001)  // If page is on ADD NEW PROFILE
        {
          // The following I took from DELETE section, but I change a few things like landing page and return label:

          EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 0);  // Record this profile as disabled

          Check3PreviousProfile:  // This is where to come back after checking if next prifile is not enabled
          // If we are in the limit, reset to 19:
          if(CurrentProfileUserSlot == 0)
          {
          CurrentProfileUserSlot = 19;  // Reset
          }
          else
          {
            CurrentProfileUserSlot = CurrentProfileUserSlot -1;  // Go to previous profile
          }

          // Match the correct Profile number to the Profile slot:
          CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];
    
          // Check if the previous profile slot is active:
          if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector]) != 1)  // If the current profile slot is not active
          {
            goto Check3PreviousProfile;  // Return to check the previous profile slot to see if it's active
          }
          // If the current profile slot is active, is going to continue the code from here

          EEPROM.write(EEPROM_AddressCurrentProfileSlot, CurrentProfileUserSlot);  // Save in the EEPROM the profile 1 name

          ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        
          ScreenPage = 100;  // Go to normal screen
          SelectorPosition = 0;  // Reset selector position to 0
          goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
        // End of delete section
          
        }

        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to normal screen
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      
      if(SelectorPosition == 48)  // If selector is in SAVE
      {
        // Before recording the amount of characters, we need to consider that
        // If the final character is an empty space, we should ignore that and
        // not count it as a character. Because of this, we check if the final
        // character is a SPACE and remove 1 from the AmountOfCharacters.
        while(ProfileTitleTemporal[AmountOfCharProfileName -1] == ' ' && AmountOfCharProfileName > 1)  // If last character is a SPACE, and go back and check again until the last digit is not SPACE. Also the amount of characters has to be more than 1 to avoid going to 0
        {
          //Serial.print(AmountOfCharProfileName);
          //Serial.print("-");
          AmountOfCharProfileName = AmountOfCharProfileName -1;  // Remove 1 from amount of characters
          //Serial.print(AmountOfCharProfileName);
        }

        // EEPROM write profile name:
        EEPROM.write(EEPROM_AddressName0Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[0]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName1Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[1]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName2Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[2]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName3Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[3]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName4Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[4]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName5Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[5]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName6Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[6]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName7Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[7]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName8Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[8]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName9Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[9]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName10Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[10]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName11Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[11]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName12Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[12]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName13Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[13]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName14Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[14]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName15Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[15]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName16Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[16]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName17Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[17]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName18Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[18]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName19Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[19]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressName20Profile[CurrentProfileEEPROMSector], ProfileTitleTemporal[20]);  // Save in the EEPROM the profile 1 name
        EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 1);  // Save in the EEPROM the current profile 0 is active
        
        // Write the amount of characters:
        EEPROM.write(EEPROM_AddressAmountOfCharTittleProfile[CurrentProfileEEPROMSector], AmountOfCharProfileName);  // Write the amount of characters

        
        // Before setting which page to go, we need to check if we are editing a profile
        // or creating a new profile. If we are creating a new profile, I want to go to
        // Tare.
        if(ScreenPage == 2201)  // If we are in page edit profile
        {
          ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
          ScreenPage = 100;  // Go to normal screen
        }
        else if(ScreenPage == 3001)  // If we are in page new profile
        {
          ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
          ScreenPage = 2111;  // Go to page Tare
        }
        
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }  // End of if selector is in SAVE
    }  // End of if Enter button is press
  }  // End of if we are in page EDIT PROFILE NAME





  if(ScreenPage == 2301)  // If we are in page Delete profile
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press


    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }

      if(SelectorPosition == 1)  // If selector is in DELETE
      {
        EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 0);  // Record this profile as disabled

        Check2PreviousProfile:  // This is where to come back after checking if next prifile is not enabled
        // If we are in the limit, reset to 19:
        if(CurrentProfileUserSlot == 0)
        {
          CurrentProfileUserSlot = 19;  // Reset
        }
        else
        {
          CurrentProfileUserSlot = CurrentProfileUserSlot -1;  // Go to previous profile
        }

        // Match the correct Profile number to the Profile slot:
        CurrentProfileEEPROMSector = UserProfileSlotSequence[CurrentProfileUserSlot];
    
        // Check if the previous profile slot is active:
        if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector]) != 1)  // If the current profile slot is not active
        {
          goto Check2PreviousProfile;  // Return to check the previous profile slot to see if it's active
        }
        // If the current profile slot is active, is going to continue the code from here

        EEPROM.write(EEPROM_AddressCurrentProfileSlot, CurrentProfileUserSlot);  // Save in the EEPROM the profile 1 name

        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        
        ScreenPage = 2311;  // Go to page Profile has been deleted
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }  // End of if selector is in CONTINUE
    }  // End of if Enter button is press
  }  // End of if we are in page Delete profile




  // We use the same code for multiple pages because all this pages do the same of
  // if you press enter, go to the page normal:
  if(ScreenPage == 2311 || ScreenPage == 2312 || ScreenPage == 3012 || ScreenPage == 2131 || ScreenPage == 4071)  // If we are in page: "profile has been deleted", or page "you can't delete the last profile", or "no more space available", or "page Tare 3", or "calibration 8"
  {
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
      ScreenPage = 100;  // Go to page normal
      SelectorPosition = 0;  // Reset selector position to 0
      goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
    }  // End of if Enter button is press
  }  // End of if we are in page profile has been deleted





  if(ScreenPage == 3201)  // If we are in page OPTIONS
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // Move selector backwards:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 3)// If selector is in value 2
      {
        SelectorPosition = 2;  // Put selector in value 1
      }
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Move selector forward:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else if(SelectorPosition == 1)// If selector is in value 1
      {
        SelectorPosition = 2;  // Put selector in value 2
      }
      else if(SelectorPosition == 2)// If selector is in value 2
      {
        SelectorPosition = 3;  // Put selector in value 3
      }
      else if(SelectorPosition == 3)// If selector is in value 2
      {
        SelectorPosition = 0;  // Put selector in value 3
      }
      
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in DEADZONE
      {
        ScreenPage = 3210;  // Go to page DEADZONE
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in FULL CALIBRATION
      {
        ScreenPage = 4001;  // Go to page FULL CALIBRATION
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 2)  // If selector is in FACTORY RESET
      {
        ScreenPage = 3310;  // Go to FACTORY RESET
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }

      if(SelectorPosition == 3)  // If selector is in GO BACK
      {
        ScreenPage = 1001;  // Go to main menu
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page OPTIONS





  if(ScreenPage == 3210)  // If we are in page Deadzone
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // Decrease deadzone value:
      // If we are in 0, go to the maximum allowed value:
      if(DeadzoneSamples == 0)
      {
        DeadzoneSamples = 50;  // Set deadzone as the maximum allowed
      }
      else
      {
        // Move weight value backwards:
        DeadzoneSamples = DeadzoneSamples -1;  // Decrease value of selected weight
      }
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Increase deadzone value:
      // If deadzone is at the limit:
      if(DeadzoneSamples == 50)
      {
        DeadzoneSamples = 0;  // Set deadzone with the minimum value
      }
      else
      {
        DeadzoneSamples = DeadzoneSamples +1;  // Increase value of selected weight
      }
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      // Save in the EEPROM the amount of deadzone:
      EEPROM.write(EEPROM_AddressDeadzoneAmount, DeadzoneSamples);

      ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
      ScreenPage = 100;  // Go to page normal
      SelectorPosition = 0;  // Reset selector position to 0
      goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
    }  // End of if Enter button is press
  }  // End of if we are in page Deadzone





  if(ScreenPage == 4001)  // If we are in page Full Calibration intro
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CONTINUE
      {
        ScreenPage = 4011;  // Go to page Full Calibration 2
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Full Calibration intro





  if(ScreenPage == 4011)  // If we are in page Full calibration 2
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CONTINUE
      {
        ScreenPage = 4021;  // Go to page Full Calibration 3
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in CANCEL
      {
        ScreenPage = 4001;  // Go to page Full Calibration intro
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Full calibration 2





  if(ScreenPage == 4021)  // If we are in page Full Calibration 3
  {
    // Left button:
    if(LeftButtonActivationStatePulses == 1)  // If left button is press
    {
      // Move weight value backwards:
      TemporalFullSpoolWeight = TemporalFullSpoolWeight -1;  // Decrease value of selected weight
    }  // End of if left button is press


    // Right button:
    if(RightButtonActivationStatePulses == 1)  // If right button is press
    {
      // Move weight value forward:
      TemporalFullSpoolWeight = TemporalFullSpoolWeight +1;  // Increase value of selected weight
    }  // End of if right button is press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      ScreenPage = 4031;  // Go to page Full Calibration 4
      SelectorPosition = 0;  // Reset selector position to 0
      goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
    }  // End of if Enter button is press
  }  // End of if we are in page Full Calibration 3





  if(ScreenPage == 4031)  // If we are in page Full calibration 4
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CONTINUE
      {
        ScreenPage = 4040;  // Go to page Full Calibration 5
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Full calibration 4





  if(ScreenPage == 4051)  // If we are in page Full calibration 6
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press

 
    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CONTINUE
      {
        ScreenPage = 4060;  // Go to page Full Calibration 7
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
      if(SelectorPosition == 1)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Full calibration 6





  if(ScreenPage == 3310)  // If we are in page Factory Reset
  {
    // Left and right buttons:
    if(LeftButtonActivationStatePulses == 1 || RightButtonActivationStatePulses == 1)  // If left or right buttons are press
    {
      // Alternate position of the selector:
      if(SelectorPosition == 0)  // If selector is in value 0
      {
        SelectorPosition = 1;  // Put selector in value 1
      }
      else // If selector is in value 1
      {
        SelectorPosition = 0;  // Put selector in value 0
      }
    }  // End of if left or right button are press


    // Enter button:
    if(EnterButton1PulseOnly == 1)  // If Enter button is press
    {
      if(SelectorPosition == 0)  // If selector is in CANCEL
      {
        ResetSmoothing();  // Reset smoothing and deadzone variables to start calculating from scratch
        ScreenPage = 100;  // Go to page normal
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }

      if(SelectorPosition == 1)  // If selector is in DELETE
      {
        ScreenPage = 3311;  // Go to page Factory Reset 2
        SelectorPosition = 0;  // Reset selector position to 0
        goto AfterPageSelection;  // Skip the rest part of the code to check for button presses
      }
    }  // End of if Enter button is press
  }  // End of if we are in page Factory Reset


  // End of button excecution of actions

  AfterPageSelection:  // When we select a new page, we skip the code until this part
                       // to avoid executing other commands to go to other pages.












  // Do a few things in Normal page:
  if(ScreenPage == 100)  // If we are in page normal
  {
    // Use the saved deadzone only if we are in page normal:
    DeadzoneSamples = DeadzoneAmountSet;

    // Read the amount of characters for profile name from EEPROM:
    AmountOfCharProfileName = EEPROM.read(EEPROM_AddressAmountOfCharTittleProfile[CurrentProfileEEPROMSector]);

    
    // I had a problem that if EEPROM was recorded a higher value than 21, it would glitch
    // so now I check if it's over 21 and if it is I'll out in the limit:
    if(AmountOfCharProfileName > 21 || AmountOfCharProfileName == 0)  // If amount of characters is over limits
    {
      EEPROM.write(EEPROM_AddressName0Profile[CurrentProfileEEPROMSector], 'P');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName1Profile[CurrentProfileEEPROMSector], 'R');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName2Profile[CurrentProfileEEPROMSector], 'O');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName3Profile[CurrentProfileEEPROMSector], 'F');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName4Profile[CurrentProfileEEPROMSector], 'I');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName5Profile[CurrentProfileEEPROMSector], 'L');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName6Profile[CurrentProfileEEPROMSector], 'E');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName7Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName8Profile[CurrentProfileEEPROMSector], 'N');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName9Profile[CurrentProfileEEPROMSector], 'A');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName10Profile[CurrentProfileEEPROMSector], 'M');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName11Profile[CurrentProfileEEPROMSector], 'E');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName12Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName13Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName14Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName15Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName16Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName17Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName18Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName19Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name
      EEPROM.write(EEPROM_AddressName20Profile[CurrentProfileEEPROMSector], ' ');  // Save in the EEPROM the profile 1 name

      // Write the amount of characters:
      EEPROM.write(EEPROM_AddressAmountOfCharTittleProfile[CurrentProfileEEPROMSector], 12);  // Save in the EEPROM the profile 1 name
      AmountOfCharProfileName = 12;
      
      EEPROM.write(EEPROM_AddressActiveOrDisabledProfile[CurrentProfileEEPROMSector], 1);  // Save in the EEPROM the current profile 0 is active
    }  // End of if amount of characters is over limits
  }  // End of if we are in page normal










  // Weight deadzone:
  DeadzoneSamples = DeadzoneSamples +1;  // I want the dead zone to be expressed as 0 when no deadzone, 1 for +/-1, etc
                                         // but the way it works is 1 is no deadzone and, 2 is +/-1, so I use the value I want and add 1 before the calculation
  
  WeightWithDeadzone = WeightWithDeadzone + ((SmoothedWeight - WeightWithDeadzone)/DeadzoneSamples);  // Smooth the value of temp sensor.

  // Restore the deadzone value as the one that makes more sense:
  DeadzoneSamples = DeadzoneSamples -1;  // I want the dead zone to be expressed as 0 when no deadzone, 1 for +/-1, etc
                                         // but the way it works is 1 is no deadzone and, 2 is +/-1, so I use the value I want and add 1 before the calculation





  // The way the filament is fet to the extruder is by pulling in quick motions so we get a peak weight
  // and then there's a slack so there's a drop in measured weight. The deadzone itself helps minimizing
  // this effect, but I want the lowest value of the edge of the deadzone so I have to show the weight
  // with deadzone minus the amount of deadzone.
  FinalWeightToShow = WeightWithDeadzone - DeadzoneSamples;  // Remove the amount of deadzone to the weight 





  // Make sure if weight close to 0, show 0:
  if(SmoothedWeight < MinimumWeightAllowed || FinalWeightToShow < MinimumWeightAllowed)
  {
    FinalWeightToShow = 0;
  }





  // Read profile name from EEPROM if we are in page normal:
  if(ScreenPage == 100)  // If we are in page normal
  {
    // Read profile 0 name from EEPROM:
    ProfileTitleTemporal[0] = EEPROM.read(EEPROM_AddressName0Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[1] = EEPROM.read(EEPROM_AddressName1Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[2] = EEPROM.read(EEPROM_AddressName2Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[3] = EEPROM.read(EEPROM_AddressName3Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[4] = EEPROM.read(EEPROM_AddressName4Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[5] = EEPROM.read(EEPROM_AddressName5Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[6] = EEPROM.read(EEPROM_AddressName6Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[7] = EEPROM.read(EEPROM_AddressName7Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[8] = EEPROM.read(EEPROM_AddressName8Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[9] = EEPROM.read(EEPROM_AddressName9Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[10] = EEPROM.read(EEPROM_AddressName10Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[11] = EEPROM.read(EEPROM_AddressName11Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[12] = EEPROM.read(EEPROM_AddressName12Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[13] = EEPROM.read(EEPROM_AddressName13Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[14] = EEPROM.read(EEPROM_AddressName14Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[15] = EEPROM.read(EEPROM_AddressName15Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[16] = EEPROM.read(EEPROM_AddressName16Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[17] = EEPROM.read(EEPROM_AddressName17Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[18] = EEPROM.read(EEPROM_AddressName18Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[19] = EEPROM.read(EEPROM_AddressName19Profile[CurrentProfileEEPROMSector]);
    ProfileTitleTemporal[20] = EEPROM.read(EEPROM_AddressName20Profile[CurrentProfileEEPROMSector]);

    // Reset text cursor:
    EditTextCursorPosition = 0;
  }  // End of if we are in page normal
    




  // Profile title - Transfer from array to final words:
  // We need to consider the amount of characters before transfer so
  // we transer only the amount recorded.

  if(AmountOfCharProfileName == 0)  // If profile has 0 character
  {
    strcpy(CurrentProfileTitle,"");  // Clear array so nothing shows up
  }
  if(AmountOfCharProfileName >= 1)  // If profile has 1 character or more
  {
    CurrentProfileTitle[0] = ProfileTitleTemporal[0];  // Write character from array 0
    CurrentProfileTitle[1] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 2)  // If profile has 2 characters or more
  {
    CurrentProfileTitle[1] = ProfileTitleTemporal[1];  // Write character from array 1
    CurrentProfileTitle[2] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 3)  // If profile has 3 characters or more
  {
    CurrentProfileTitle[2] = ProfileTitleTemporal[2];  // Write character from array 2
    CurrentProfileTitle[3] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 4)  // If profile has 4 characters or more
  {
    CurrentProfileTitle[3] = ProfileTitleTemporal[3];  // Write character from array 3
    CurrentProfileTitle[4] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 5)  // If profile has 5 characters or more
  {
    CurrentProfileTitle[4] = ProfileTitleTemporal[4];  // Write character from array 4
    CurrentProfileTitle[5] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 6)  // If profile has 6 characters or more
  {
    CurrentProfileTitle[5] = ProfileTitleTemporal[5];  // Write character from array 5
    CurrentProfileTitle[6] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 7)  // If profile has 7 characters or more
  {
    CurrentProfileTitle[6] = ProfileTitleTemporal[6];  // Write character from array 6
    CurrentProfileTitle[7] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 8)  // If profile has 8 characters or more
  {
    CurrentProfileTitle[7] = ProfileTitleTemporal[7];  // Write character from array 7
    CurrentProfileTitle[8] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 9)  // If profile has 9 characters or more
  {
    CurrentProfileTitle[8] = ProfileTitleTemporal[8];  // Write character from array 8
    CurrentProfileTitle[9] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 10)  // If profile has 10 characters or more
  {
    CurrentProfileTitle[9] = ProfileTitleTemporal[9];  // Write character from array 9
    CurrentProfileTitle[10] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 11)  // If profile has 11 characters or more
  {
    CurrentProfileTitle[10] = ProfileTitleTemporal[10];  // Write character from array 10
    CurrentProfileTitle[11] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 12)  // If profile has 12 characters or more
  {
    CurrentProfileTitle[11] = ProfileTitleTemporal[11];  // Write character from array 11
    CurrentProfileTitle[12] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 13)  // If profile has 13 characters or more
  {
    CurrentProfileTitle[12] = ProfileTitleTemporal[12];  // Write character from array 12
    CurrentProfileTitle[13] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 14)  // If profile has 14 characters or more
  {
    CurrentProfileTitle[13] = ProfileTitleTemporal[13];  // Write character from array 13
    CurrentProfileTitle[14] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 15)  // If profile has 15 characters or more
  {
    CurrentProfileTitle[14] = ProfileTitleTemporal[14];  // Write character from array 14
    CurrentProfileTitle[15] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 16)  // If profile has 16 characters or more
  {
    CurrentProfileTitle[15] = ProfileTitleTemporal[15];  // Write character from array 15
    CurrentProfileTitle[16] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 17)  // If profile has 17 characters or more
  {
    CurrentProfileTitle[16] = ProfileTitleTemporal[16];  // Write character from array 16
    CurrentProfileTitle[17] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 18)  // If profile has 18 characters or more
  {
    CurrentProfileTitle[17] = ProfileTitleTemporal[17];  // Write character from array 17
    CurrentProfileTitle[18] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 19)  // If profile has 19 characters or more
  {
    CurrentProfileTitle[18] = ProfileTitleTemporal[18];  // Write character from array 18
    CurrentProfileTitle[19] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 20)  // If profile has 20 characters or more
  {
    CurrentProfileTitle[19] = ProfileTitleTemporal[19];  // Write character from array 19
    CurrentProfileTitle[20] = '\0';  // Terminator to clear the rest of characters
  }
  if(AmountOfCharProfileName >= 21)  // If profile has 21 characters or more
  {
    CurrentProfileTitle[20] = ProfileTitleTemporal[20];  // Write character from array 20
    CurrentProfileTitle[21] = '\0';  // Terminator to clear the rest of characters
  }
  







  // Center title text:
  // The X position of the text depends on the amount of characters
  // so we check how many characters the profile title has so we can put the
  // correct X position so the text is centered on the display.

  if(AmountOfCharProfileName == 1)  // If profile has 1 character
  {
    TitleXPosition = 61;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 2)  // If profile has 2 characters
  {
    TitleXPosition = 58;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 3)  // If profile has 3 characters
  {
    TitleXPosition = 55;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 4)  // If profile has 4 characters
  {
    TitleXPosition = 52;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 5)  // If profile has 5 characters
  {
    TitleXPosition = 49;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 6)  // If profile has 6 characters
  {
    TitleXPosition = 46;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 7)  // If profile has 7 characters
  {
    TitleXPosition = 43;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 8)  // If profile has 8 characters
  {
    TitleXPosition = 40;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 9)  // If profile has 9 characters
  {
    TitleXPosition = 37;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 10)  // If profile has 10 characters
  {
    TitleXPosition = 34;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 11)  // If profile has 11 characters
  {
    TitleXPosition = 31;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 12)  // If profile has 12 characters
  {
    TitleXPosition = 28;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 13)  // If profile has 13 characters
  {
    TitleXPosition = 25;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 14)  // If profile has 14 characters
  {
    TitleXPosition = 22;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 15)  // If profile has 15 characters
  {
    TitleXPosition = 19;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 16)  // If profile has 16 characters
  {
    TitleXPosition = 16;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 17)  // If profile has 17 characters
  {
    TitleXPosition = 13;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 18)  // If profile has 18 characters
  {
    TitleXPosition = 10;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 19)  // If profile has 19 characters
  {
    TitleXPosition = 7;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 20)  // If profile has 20 characters
  {
    TitleXPosition = 4;  // Cursor X position for title
  }
  else if(AmountOfCharProfileName == 21)  // If profile has 21 characters
  {
    TitleXPosition = 1;  // Cursor X position for title
  }





  // Set the text cursor X position based on the cursor character position:
  if(EditTextCursorPosition == 0)
  {
    TextCursorPositionX = 0;
  }
  else if(EditTextCursorPosition == 1)
  {
    TextCursorPositionX = 6;
  }
  else if(EditTextCursorPosition == 2)
  {
    TextCursorPositionX = 12;
  }
  else if(EditTextCursorPosition == 3)
  {
    TextCursorPositionX = 18;
  }
  else if(EditTextCursorPosition == 4)
  {
    TextCursorPositionX = 24;
  }
  else if(EditTextCursorPosition == 5)
  {
    TextCursorPositionX = 30;
  }
  else if(EditTextCursorPosition == 6)
  {
    TextCursorPositionX = 36;
  }
  else if(EditTextCursorPosition == 7)
  {
    TextCursorPositionX = 42;
  }
  else if(EditTextCursorPosition == 8)
  {
    TextCursorPositionX = 48;
  }
  else if(EditTextCursorPosition == 9)
  {
    TextCursorPositionX = 54;
  }
  else if(EditTextCursorPosition == 10)
  {
    TextCursorPositionX = 60;
  }
  else if(EditTextCursorPosition == 11)
  {
    TextCursorPositionX = 66;
  }
  else if(EditTextCursorPosition == 12)
  {
    TextCursorPositionX = 72;
  }
  else if(EditTextCursorPosition == 13)
  {
    TextCursorPositionX = 78;
  }
  else if(EditTextCursorPosition == 14)
  {
    TextCursorPositionX = 84;
  }
  else if(EditTextCursorPosition == 15)
  {
    TextCursorPositionX = 90;
  }
  else if(EditTextCursorPosition == 16)
  {
    TextCursorPositionX = 96;
  }
  else if(EditTextCursorPosition == 17)
  {
    TextCursorPositionX = 102;
  }
  else if(EditTextCursorPosition == 18)
  {
    TextCursorPositionX = 108;
  }
  else if(EditTextCursorPosition == 19)
  {
    TextCursorPositionX = 114;
  }
  else if(EditTextCursorPosition == 20)
  {
    TextCursorPositionX = 120;
  }










  /////////////
  // DISPLAY //
  /////////////

  // Print Display:
  u8g.firstPage(); // Beginning of the picture loop
  do  // Include all you want to show on the display:
  {
    // Display - Welcome page: If no full calibration is detected, we show this:
    if(ScreenPage == 0)  // If page is on welcome page
    {
      u8g.setFont(u8g_font_profont12);  // Set small font
      u8g.drawStrP(0, 8, TextTheloadcell);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textrequirescalibration);  // (x,y,"Text")
      u8g.drawStrP(0, 28, TextDoyouwanttostart);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textthefullcalibration);  // (x,y,"Text")
      u8g.drawStrP(0, 48, Textprocess);  // (x,y,"Text")

      u8g.drawStrP(22, 62, TextNO);  // (x,y,"Text")  Correctly centered
      u8g.drawStrP(86, 62, TextSTART);  // (x,y,"Text")  Correctly centered

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on welcome page





    // Display - Normal screen
    if(ScreenPage == 100)  // If page is on profile normal screen
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")

      // Preparing to print weight:
      // Since the weight X position depends on how big is the value, we need to check
      // and change the X position acordently. This is to print in the center.
      if(FinalWeightToShow < 10000)  // If weight is below 9999
      {
        WeightXPosition = 26;  // Change X position of the weight digits
        gSymbolXPosition = 105;  // Change the X position of the g symbol (5 pixels away from weight)
      }
      if(FinalWeightToShow < 1000)
      {
        WeightXPosition = 36;  // Change X position of the weight digits
        gSymbolXPosition = 96;  // Change the X position of the g symbol (5 pixels away from weight)
      }
      if(FinalWeightToShow < 100)
      {
        WeightXPosition = 45;  // Change X position of the weight digits
        gSymbolXPosition = 86;  // Change the X position of the g symbol (5 pixels away from weight)
      }
      if(FinalWeightToShow < 10)
      {
        WeightXPosition = 54;  // Change X position of the weight digits
        gSymbolXPosition = 76;  // Change the X position of the g symbol (5 pixels away from weight)
      }

      // Print weight:
      u8g.setPrintPos(WeightXPosition, WeightYPosition);  // (x,y)
      u8g.setFont(u8g_font_fur25n);  // Set big number (only) font
      u8g.print(FinalWeightToShow);  // Print weight

      // Print "g" symbol:
      u8g.setFont(u8g_font_profont12);  // Set small font
      u8g.drawStrP(gSymbolXPosition, WeightYPosition, Textg);  // (x,y,"Text")

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center
    
      //u8g.drawLine(57, 0, 57, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(69, 0, 69, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
      
      //u8g.drawLine(52, 0, 52, 63);  // Draw a line (x0,y0,x1,y1) 2 char left
      //u8g.drawLine(74, 0, 74, 63);  // Draw a line (x0,y0,x1,y1) 2 char right
      
      //u8g.drawLine(0, 0, 0, 63);  // Draw a line (x0,y0,x1,y1) Vertical left
      //u8g.drawLine(127, 0, 127, 63);  // Draw a line (x0,y0,x1,y1) Vertical right
      //u8g.drawLine(0, 0, 127, 0);  // Draw a line (x0,y0,x1,y1) Horizontal top
      //u8g.drawLine(0, 63, 127, 63);  // Draw a line (x0,y0,x1,y1) Horizontal bottom

    }  // End of if we are in normal page





    // Display - Main Menu
    if(ScreenPage == 1001)  // If page is on profile normal screen
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")
      u8g.drawStrP(37, 18, TextMAINMENU);  // (x,y,"Text")

      u8g.drawStrP(0, 29, TextEDITTHISPROFILE);  // (x,y,"Text")
      u8g.drawStrP(0, 40, TextADDNEWPROFILEMenu);  // (x,y,"Text")
      u8g.drawStrP(0, 51, TextOPTIONSMenu);  // (x,y,"Text")
      u8g.drawStrP(0, 62, TextGOBACKMenu);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(0, 19, 127, SelectBoxHeight);  // Draw a square for EDIT THIS PROFILE (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, 30, 127, SelectBoxHeight);  // Draw a square for ADD NEW PROFILE (x,y,width,height)
      }
      if(SelectorPosition == 2)
      {
        u8g.drawFrame(0, 41, 127, SelectBoxHeight);  // Draw a square for FULL CALIBRATION (x,y,width,height)
      }
      if(SelectorPosition == 3)
      {
        u8g.drawFrame(0, SelectBoxYPosition, 127, SelectBoxHeight);  // Draw a square for GO BACK (x,y,width,height)
      }
    }  // End of if we are in normal page





    // Display - Main Menu > EDIT PROFILE
    if(ScreenPage == 2101)  // If page is on EDIT THIS PROFILE
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")
      u8g.drawStrP(28, 18, TextEDITPROFILE);  // (x,y,"Text")

      u8g.drawStrP(0, 29, TextTAREZERO);  // (x,y,"Text")
      u8g.drawStrP(0, 40, TextEDITPROFILENAMEMenu);  // (x,y,"Text")
      u8g.drawStrP(0, 51, TextDELETETHISPROFILE);  // (x,y,"Text")
      u8g.drawStrP(0, 62, TextGOBACKMenu);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(0, 19, 127, SelectBoxHeight);  // Draw a square for EDIT THIS PROFILE (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, 30, 127, SelectBoxHeight);  // Draw a square for ADD NEW PROFILE (x,y,width,height)
      }
      if(SelectorPosition == 2)
      {
        u8g.drawFrame(0, 41, 127, SelectBoxHeight);  // Draw a square for FULL CALIBRATION (x,y,width,height)
      }
      if(SelectorPosition == 3)
      {
        u8g.drawFrame(0, SelectBoxYPosition, 127, SelectBoxHeight);  // Draw a square for GO BACK (x,y,width,height)
      }
    }  // End of if we are in EDIT THIS PROFILE





    // Display - Main Menu > EDIT THIS PROFILE > TARE 1
    if(ScreenPage == 2111)  // If page is on TARE 1
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")
            
      u8g.drawStrP(0, 18, TextPlaceintheholder);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Texttheemptyspoolyou);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textwanttouseforthis);  // (x,y,"Text")
      u8g.drawStrP(0, 48, Textprofile);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(77, 62, TextCONTINUE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on Tare 1





    // Display - Main Menu > EDIT THIS PROFILE > TARE 2
    if(ScreenPage == 2120)  // If page is on TARE 2
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")
      
      u8g.drawStrP(0, 18, TextSavingtheweightof);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Texttheemptyspool);  // (x,y,"Text")
      u8g.drawStrP(0, 38, TextPleasedonttouch);  // (x,y,"Text")
      u8g.drawStrP(0, 48, Textanything);  // (x,y,"Text")

      // Progress bar:
      u8g.drawFrame(0, SelectBoxYPosition, 127, SelectBoxHeight);  // Draw a square for frame (x,y,width,height)

      // Print the inside of the progress bar:
      u8g.drawBox(2, 54, ProgressBarCounter, SelectBoxHeight -4);  // Draw a filled square for bar (x,y,width,height)

      // Fill progress bar gradually:
      if(ProgressBarCounter == 123)  // If the progress bar counter reach full limit
      {
        writeIntIntoEEPROM(EEPROM_AddressEmptySpoolMeasuredWeightProfile[CurrentProfileEEPROMSector], HolderWeight);  // Write to EEPROM the measured weight when spool is empty in Profile Tare
        
        ProgressBarCounter = 0;  // Reset counter
        ScreenPage = 2131;  // Go to page Tare 3
      }
      else  // If progress bar is not yet full
      {
        if(ProgressBarSpeedCounter == ProgressBarSpeedAmount)  // If we waited enough
        {
          ProgressBarCounter = ProgressBarCounter + 1;  // Increase counter by 1
          ProgressBarSpeedCounter = 0;  // Reset counter
        }
        else  // If we are not yet in the limit of the counter
        {
          ProgressBarSpeedCounter = ProgressBarSpeedCounter + 1;  // Increase counter
        }
      }  // End of if progress bar is not yet full
    }  // End of if page is on Tare 2





    // Display - Main Menu > EDIT THIS PROFILE > TARE 3
    if(ScreenPage == 2131)  // If page is on TARE 3
    {
      // Print profile tittle:
      u8g.drawStr(TitleXPosition, 8, CurrentProfileTitle);  // (x,y,"Text")
            
      u8g.drawStrP(0, 18, TextTheweightofthe);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Textemptyspoolhasbeen);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textsaved);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center
    
      //u8g.drawLine(37, 0, 37, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(89, 0, 89, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
          
    }  // End of if page is on Tare 3





    // Display - Main Menu > EDIT THIS PROFILE > EDIT PROFILE NAME
    if(ScreenPage == 2201 || ScreenPage == 3001)  // If page is on EDIT PROFILE NAME or NEW PROFILE
    {
      // Print profile tittle:
      // If we are editing an existing profile, we show the text for that, and
      // if we are creating a new profile, we show that other text:
      if(ScreenPage == 2201)  // If page is on EDIT PROFILE NAME
      {
        u8g.drawStrP(13, 8, TextEDITPROFILENAME);  // (x,y,"Text")
      }
      else if(ScreenPage == 3001)  // If page is on ADD NEW PROFILE
      {
        u8g.drawStrP(19, 8, TextADDNEWPROFILE);  // (x,y,"Text")
      }

      // Profile name, so far:
      u8g.drawStr(0, 28, CurrentProfileTitle);  // (x,y,"Text")

      // Text Cursor:
      if(BlinkingCursorCounter == BlinkingCursorAmount)  // If counter reach limit
      {
        if(BlinkingCursorState == 0)  // If text cursor is OFF
        {
          BlinkingCursorState = 1;  // Turn text cursor ON
        }
        else  // If text cursor is ON
        {
          BlinkingCursorState = 0;  // Turn text cursor OFF
        }
        BlinkingCursorCounter = 0;  // Reset counter to start over
      }
      else  // If counter for cursor is not yet at the limit
      {
        BlinkingCursorCounter = BlinkingCursorCounter + 1;  // Increase counter by 1
      }

      // Print the line for the text cursor only if the state is 1:
      if(BlinkingCursorState == 1)
      {
        u8g.drawLine(TextCursorPositionX, 30, TextCursorPositionX + 4, 30);  // Draw a line (x0,y0,x1,y1) Cursor
      }

      // Print letters depending on the position of the selector:
      if(SelectorPosition == 0)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterA);  // (x,y,"Text")
      }
      else if(SelectorPosition == 1)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterB);  // (x,y,"Text")
      }
      else if(SelectorPosition == 2)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterC);  // (x,y,"Text")
      }
      else if(SelectorPosition == 3)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterD);  // (x,y,"Text")
      }
      else if(SelectorPosition == 4)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterE);  // (x,y,"Text")
      }
      else if(SelectorPosition == 5)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterF);  // (x,y,"Text")
      }
      else if(SelectorPosition == 6)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterG);  // (x,y,"Text")
      }
      else if(SelectorPosition == 7)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterH);  // (x,y,"Text")
      }
      else if(SelectorPosition == 8)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterI);  // (x,y,"Text")
      }
      else if(SelectorPosition == 9)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterJ);  // (x,y,"Text")
      }
      else if(SelectorPosition == 10)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterK);  // (x,y,"Text")
      }

      else if(SelectorPosition == 11)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterL);  // (x,y,"Text")
      }
      else if(SelectorPosition == 12)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterM);  // (x,y,"Text")
      }
      else if(SelectorPosition == 13)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterN);  // (x,y,"Text")
      }
      else if(SelectorPosition == 14)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterO);  // (x,y,"Text")
      }
      else if(SelectorPosition == 15)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterP);  // (x,y,"Text")
      }
      else if(SelectorPosition == 16)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterQ);  // (x,y,"Text")
      }
      else if(SelectorPosition == 17)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterR);  // (x,y,"Text")
      }
      else if(SelectorPosition == 18)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterS);  // (x,y,"Text")
      }
      else if(SelectorPosition == 19)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterT);  // (x,y,"Text")
      }
      else if(SelectorPosition == 20)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterU);  // (x,y,"Text")
      }
      else if(SelectorPosition == 21)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterV);  // (x,y,"Text")
      }
      else if(SelectorPosition == 22)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterW);  // (x,y,"Text")
      }
      else if(SelectorPosition == 23)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterX);  // (x,y,"Text")
      }
      else if(SelectorPosition == 24)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterY);  // (x,y,"Text")
      }
      else if(SelectorPosition == 25)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterZ);  // (x,y,"Text")
      }
      else if(SelectorPosition == 26)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL1);  // (x,y,"Text")
      }
      else if(SelectorPosition == 27)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL2);  // (x,y,"Text")
      }
      else if(SelectorPosition == 28)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL3);  // (x,y,"Text")
      }
      else if(SelectorPosition == 29)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL4);  // (x,y,"Text")
      }
      else if(SelectorPosition == 30)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL5);  // (x,y,"Text")
      }
      else if(SelectorPosition == 31)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL6);  // (x,y,"Text")
      }
      else if(SelectorPosition == 32)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL7);  // (x,y,"Text")
      }
      else if(SelectorPosition == 33)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterSYMBOL8);  // (x,y,"Text")
      }
      else if(SelectorPosition == 34)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER0);  // (x,y,"Text")
      }
      else if(SelectorPosition == 35)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER1);  // (x,y,"Text")
      }
      else if(SelectorPosition == 36)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER2);  // (x,y,"Text")
      }
      else if(SelectorPosition == 37)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER3);  // (x,y,"Text")
      }
      else if(SelectorPosition == 38)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER4);  // (x,y,"Text")
      }
      else if(SelectorPosition == 39)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER5);  // (x,y,"Text")
      }
      else if(SelectorPosition == 40)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER6);  // (x,y,"Text")
      }
      else if(SelectorPosition == 41)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER7);  // (x,y,"Text")
      }
      else if(SelectorPosition == 42)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER8);  // (x,y,"Text")
      }
      else if(SelectorPosition == 43)
      {
        u8g.drawStrP(25, 48, ABCTextCharacterNUMBER9);  // (x,y,"Text")
      }
      else if(SelectorPosition == 44)  // SPACEBAR
      {
        //u8g.drawStrP(49, 48, ABCTextCharacterOPTION1);  // (x,y,"Text")

        u8g.drawFrame(28, 38, 71, 13);  // Draw filled square
        //u8g.setColorIndex(0);  // Print the next erasing the filled box
        u8g.drawLine(47, 46, 47, 41);  // Draw a line (x0,y0,x1,y1) 1 char left
        u8g.drawLine(79, 46, 79, 41);  // Draw a line (x0,y0,x1,y1) 1 char right
        u8g.drawLine(47, 47, 79, 47);  // Draw a line (x0,y0,x1,y1) horizontal
        //u8g.setColorIndex(1);  // Return to printing the normal way

      }
      else if(SelectorPosition == 45)  // NEXT
      {
        //u8g.drawStrP(52, 48, ABCTextCharacterOPTION2);  // (x,y,"Text")
        
        u8g.drawFrame(28, 38, 71, 13);  // Draw filled square
        //u8g.setColorIndex(0);  // Print the next erasing the filled box
        
        u8g.drawLine(52, 47, 56, 47);  // Draw a line (x0,y0,x1,y1) horizontal line to represent cursor
        u8g.drawLine(60, 44, 66, 44);  // Draw a line (x0,y0,x1,y1) horizontal line of the arrow
        u8g.drawTriangle(75,44,    67,39,    67,49);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) RIGHT ARROW

        //u8g.setColorIndex(1);  // Return to printing the normal way
        
      }
      else if(SelectorPosition == 46)  // BACKSPACE
      {
        //u8g.drawStrP(37, 48, ABCTextCharacterOPTION3);  // (x,y,"Text")

        u8g.drawFrame(28, 38, 71, 13);  // Draw filled square (x,y,width,height)

        u8g.drawBox(62, 40, 11, 9);  // Draw filled square for symbol (x,y,width,height)

        u8g.drawTriangle(54,44,    62,39,    62,49);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) LEFT ARROW

        u8g.setColorIndex(0);  // Print the next erasing the filled box
       
        u8g.drawLine(63, 41, 69, 47);  // Draw a line (x0,y0,x1,y1) X symbol
        u8g.drawLine(64, 41, 70, 47);  // Draw a line (x0,y0,x1,y1) X symbol
        
        u8g.drawLine(63, 47, 69, 41);  // Draw a line (x0,y0,x1,y1) X symbol
        u8g.drawLine(64, 47, 70, 41);  // Draw a line (x0,y0,x1,y1) X symbol
        u8g.setColorIndex(1);  // Return to printing the normal way

        
      }
      else if(SelectorPosition == 47)  // CANCEL
      {
        u8g.drawStrP(46, 48, ABCTextCharacterOPTION4);  // (x,y,"Text")
      }
      else if(SelectorPosition == 48)  // SAVE
      {
        u8g.drawStrP(52, 48, ABCTextCharacterOPTION5);  // (x,y,"Text")
      }

      // Selection box:
      // Depending on the options, the selection box size changes:
      if(SelectorPosition >= 44 && SelectorPosition <= 48)  // If the selection is in one of the options
      {
        u8g.drawFrame(28, 38, 71, 13);  // Draw a square for character on the center (x,y,width,height)
      }
      else  // If the selector is not in an option
      {
        u8g.drawFrame(58, 38, 11, 13);  // Draw a square for character on the center (x,y,width,height)
      }

      // Draw arrows:
      u8g.drawTriangle(0,43,    11,37,    11,49);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) LEFT ARROW
      u8g.drawTriangle(127,43,    116,37,    116,49);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) RIGHT ARROW

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center

      //u8g.drawLine(52, 0, 52, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(74, 0, 74, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
    
      //u8g.drawLine(37, 0, 37, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(89, 0, 89, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
          
    }  // End of if page is on edit profile name





    // Display - Main Menu > ADD PROFILE - No space available
    if(ScreenPage == 3012)  // If page is on ADD PROFILE - No space available
    {
      // Print profile tittle:
      u8g.drawStrP(0, 8, TextMemoryisfull);  // (x,y,"Text")
      u8g.drawStrP(0, 18, TextYoucantaddmore);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Textprofiles);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)
    }  // End of if page is on ADD PROFILE - No space available





    // Display - Main Menu > EDIT THIS PROFILE > DELETE THIS PROFILE
    if(ScreenPage == 2301)  // If page is on DELETE THIS PROFILE
    {
      u8g.drawStrP(0, 8, TextTheprofilenamed);  // (x,y,"Text")
      u8g.drawStr(0, 18, CurrentProfileTitle);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Textisgoingtobe);  // (x,y,"Text")
      u8g.drawStrP(0, 38, TextdeletedOK);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(83, 62, TextDELETE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on DELETE THIS PROFILE





    // Display - Main Menu > EDIT THIS PROFILE > DELETE THIS PROFILE 2
    if(ScreenPage == 2311)  // If page is on DELETE THIS PROFILE 2
    {
      // Print profile tittle:
      u8g.drawStrP(0, 8, TextTheprofilenamed);  // (x,y,"Text")
      u8g.drawStr(0, 18, CurrentProfileTitle);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Texthasbeendeleted);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center
    
      //u8g.drawLine(37, 0, 37, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(89, 0, 89, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
    }  // End of if page is on DELETE THIS PROFILE 2





    // Display - Main Menu > EDIT THIS PROFILE > DELETE THIS PROFILE 3
    if(ScreenPage == 2312)  // If page is on DELETE THIS PROFILE 3
    {
      // Print profile tittle:
      u8g.drawStrP(0, 8, TextYoucantdelete);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Texttheonlyprofile);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Textleft);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)
    }  // End of if page is on DELETE THIS PROFILE 3





    // Display - Main Menu > Options
    if(ScreenPage == 3201)  // If page is on Options
    {
      // Print profile tittle:
      u8g.drawStrP(43, 8, TextOPTIONS);  // (x,y,"Text")

      u8g.drawStrP(0, 19, TextDEADZONEMenu);  // (x,y,"Text")
      u8g.drawStrP(0, 30, TextFULLCALIBRATION);  // (x,y,"Text")
      u8g.drawStrP(0, 41, TextFACTORYRESETMenu);  // (x,y,"Text")
      u8g.drawStrP(0, 52, TextGOBACKMenu);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(0, 9, 127, SelectBoxHeight);  // Draw a square for deadzone (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, 20, 127, SelectBoxHeight);  // Draw a square for FULL CALIBRATION (x,y,width,height)
      }
      if(SelectorPosition == 2)
      {
        u8g.drawFrame(0, 31, 127, SelectBoxHeight);  // Draw a square for FACTORY RESET (x,y,width,height)
      }
      if(SelectorPosition == 3)
      {
        u8g.drawFrame(0, 42, 127, SelectBoxHeight);  // Draw a square for GO BACK (x,y,width,height)
      }
    }  // End of if we are in Options





    // Display - Main Menu > Options > Deadzone
    if(ScreenPage == 3210)  // If page is on Deadzone
    {
      // Print profile tittle:
      u8g.drawStrP(40, 8, TextDEADZONE);  // (x,y,"Text")

      // Center deadzone amount text:
      if(DeadzoneSamples * 2 < 10)
      {
        u8g.setPrintPos(61, 25);  // (x,y)
      }
      else if(DeadzoneSamples * 2 < 100)
      {
        u8g.setPrintPos(58, 25);  // (x,y)
      }
      else
      {
        u8g.setPrintPos(55, 25);  // (x,y)
      }

      u8g.print(DeadzoneSamples * 2);  // Deadzone amount set, multiply by 2 so it shows the total deadzone

      u8g.drawStrP(0, 38, TextINPUT);  // (x,y,"Text")
      u8g.setPrintPos(0, 47);  // (x,y)
      // Make sure if weight close to 0, show 0:
      if(SmoothedWeight < MinimumWeightAllowed)
      {
        SmoothedWeightToShowAsInputInDeadzone = 0;
      }
      else
      {
        SmoothedWeightToShowAsInputInDeadzone = SmoothedWeight;  // Show smoothed weight
      }
      u8g.print(SmoothedWeightToShowAsInputInDeadzone);  // Print weight with smooth without deadzone

      u8g.drawStrP(80, 38, TextOUTPUT);  // (x,y,"Text")
      u8g.setPrintPos(80, 47);  // (x,y)
      u8g.print(FinalWeightToShow);  // Print weight after deadzone

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)

      // Big arrows:
      u8g.drawTriangle(0,20,    11,14,    11,26);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) LEFT ARROW
      u8g.drawTriangle(127,20,    116,14,    116,26);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) RIGHT ARROW

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center
    
      //u8g.drawLine(37, 0, 37, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(89, 0, 89, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
    }  // End of if we are in Deadzone





    // Display full calibration intro:
    if(ScreenPage == 4001)  // If page is on full calibration intro
    {
      u8g.drawStrP(0, 8, TextThisprocedureis);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textabouttakinga);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Textreferencefromareal);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textscaletocalibrate);  // (x,y,"Text")
      u8g.drawStrP(0, 48, Texttheloadcell);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(77, 62, TextCONTINUE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on full calibration intro





    // Display full calibration > Step 1:
    if(ScreenPage == 4011)  // If page is on full calibration
    {
      u8g.drawStrP(0, 8, TextPlaceanyfullspool);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textonarealscaleand);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Texttakenoteofthe);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textexactweightgrams);  // (x,y,"Text")

      u8g.drawStrP(7, 62, TextGOBACK);  // (x,y,"Text")
      u8g.drawStrP(77, 62, TextCONTINUE);  // (x,y,"Text")
      
      // Reset variable for selecting the weight to the default value:
      TemporalFullSpoolWeight = 1250;  // This is the default value I want to start with

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on full calibration 1





    // Display full calibration > Step 2:
    if(ScreenPage == 4021)  // If page is on full calibration > Step 2
    {
      u8g.drawStrP(0, 8, TextEntertheweight);  // (x,y,"Text")
      u8g.setPrintPos(60, 35);  // (x,y)
      u8g.print(TemporalFullSpoolWeight);  // Print full spool weight selected by user in calibration
      u8g.drawStrP(90, 35, Textg);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)

      // Big arrows:
      u8g.drawTriangle(0,30,    11,24,    11,36);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) LEFT ARROW
      u8g.drawTriangle(127,30,    116,24,    116,36);  // Draw a filled triangle (x0,y0,    x1,y1,    x2,y2) RIGHT ARROW

      // Centering lines:
      // This is only used when testing centering items on the screen
      //u8g.drawLine(63, 0, 63, 63);  // Draw a line (x0,y0,x1,y1) Vertical center
    
      //u8g.drawLine(37, 0, 37, 63);  // Draw a line (x0,y0,x1,y1) 1 char left
      //u8g.drawLine(89, 0, 89, 63);  // Draw a line (x0,y0,x1,y1) 1 char right
    }  // End of if page is on full calibration 2





    // Display full calibration > Step 3:
    if(ScreenPage == 4031)  // If page is on full calibration > Step 3
    {
      u8g.drawStrP(0, 8, TextPlacethatsamefull);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textspoolintheholder);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(77, 62, TextCONTINUE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on full calibration 3





    // Display full calibration > Step 4:
    if(ScreenPage == 4040)  // If page is on full calibration > Step 4
    {
      u8g.drawStrP(0, 8, TextSavingtheweightof);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textthefullspool);  // (x,y,"Text")
      u8g.drawStrP(0, 28, TextPleasedonttouch);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textanything);  // (x,y,"Text")

      // Progress bar:
      u8g.drawFrame(0, SelectBoxYPosition, 127, SelectBoxHeight);  // Draw a square for frame (x,y,width,height)

      // Print the inside of the progress bar:
      u8g.drawBox(2, 54, ProgressBarCounter, SelectBoxHeight -4);  // Draw a filled square for bar (x,y,width,height)

      // Fill progress bar gradually:
      if(ProgressBarCounter == 123)  // If the progress bar counter reach full limit
      {
        // Transfer the value of the load cell to the temporal variable;
        TemporalFullSpoolRawValue = RawLoadCellReading;
        
        ProgressBarCounter = 0;  // Reset counter
        ScreenPage = 4051;  // Go to page Full Calibration 5
      }
      else  // If progress bar is not yet full
      {
        if(ProgressBarSpeedCounter == ProgressBarSpeedAmount)  // If we waited enough
        {
          ProgressBarCounter = ProgressBarCounter + 1;  // Increase counter by 1
          ProgressBarSpeedCounter = 0;  // Reset counter
        }
        else  // If we are not yet in the limit of the counter
        {
          ProgressBarSpeedCounter = ProgressBarSpeedCounter + 1;  // Increase counter
        }
      }  // End of if progress bar is not yet full
    }  // End of if page is on full calibration 4





    // Display full calibration > Step 5:
    if(ScreenPage == 4051)  // If page is on full calibration > Step 5
    {
      u8g.drawStrP(0, 8, TextWeightsaved);  // (x,y,"Text")
      u8g.drawStrP(0, 18, TextRemovethespoolfrom);  // (x,y,"Text")
      u8g.drawStrP(0, 28, Texttheholderleaving);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Texttheholdercompletely);  // (x,y,"Text")
      u8g.drawStrP(0, 48, Textempty);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(77, 62, TextCONTINUE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on full calibration 5





    // Display full calibration > Step 6:
    if(ScreenPage == 4060)  // If page is on full calibration > Step 6
    {
      u8g.drawStrP(0, 8, TextSavingthevalueof);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textanemptyholder);  // (x,y,"Text")
      u8g.drawStrP(0, 28, TextPleasedonttouch);  // (x,y,"Text")
      u8g.drawStrP(0, 38, Textanything);  // (x,y,"Text")

      // Progress bar:
      u8g.drawFrame(0, SelectBoxYPosition, 127, SelectBoxHeight);  // Draw a square for frame (x,y,width,height)

      // Print the inside of the progress bar:
      u8g.drawBox(2, 54, ProgressBarCounter, SelectBoxHeight -4);  // Draw a filled square for bar (x,y,width,height)

      // Fill progress bar gradually:
      if(ProgressBarCounter == 123)  // If the progress bar counter reach full limit
      {
       // Transfer the value of the load cell to the EEPROM;
       writeLongIntoEEPROM(EEPROM_AddressEmptyHolder, RawLoadCellReading);  // Write to EEPROM the raw value when holder is empty

        // Record that we have done the full calibration:
        EEPROM.write(EEPROM_AddressCalibrationDone, 1);  // Save in the EEPROM that we have done the Full calibration
                                                 // This is so when we power cycle, we don't pront the welcome screen

        writeLongIntoEEPROM(EEPROM_AddressFullSpool, TemporalFullSpoolRawValue);  // Write to EEPROM the raw value to the Full Calibration full spool
        writeLongIntoEEPROM(EEPROM_AddressFullSpoolWeight, TemporalFullSpoolWeight);  // Write to EEPROM the set weight when spool is full in Full Calibration

        ProgressBarCounter = 0;  // Reset counter
        ScreenPage = 4071;  // Go to page Full Calibration 7
      }
      else  // If progress bar is not yet full
      {
        if(ProgressBarSpeedCounter == ProgressBarSpeedAmount)  // If we waited enough
        {
          ProgressBarCounter = ProgressBarCounter + 1;  // Increase counter by 1
          ProgressBarSpeedCounter = 0;  // Reset counter
        }
        else  // If we are not yet in the limit of the counter
        {
          ProgressBarSpeedCounter = ProgressBarSpeedCounter + 1;  // Increase counter
        }
      }  // End of if progress bar is not yet full
    }  // End of if page is on full calibration 6





    // Display full calibration > Step 7:
    if(ScreenPage == 4071)  // If page is on full calibration > Step 7
    {
      // Print profile tittle:
      u8g.drawStrP(0, 8, TextThecalibrationhas);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textbeencompleted);  // (x,y,"Text")

      u8g.drawStrP(58, 62, TextOK);  // (x,y,"Text")

      // Selection box:
      u8g.drawFrame(SelectBoxXPositionCenter, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for OK (x,y,width,height)
    }  // End of if page is on full calibration 7





    // Display Options > Factory Reset:
    if(ScreenPage == 3310)  // If page is on Factory Reset
    {
      u8g.drawStrP(0, 8, TextFactoryreseterases);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textallyouruserdata);  // (x,y,"Text")

      u8g.drawStrP(10, 62, TextCANCEL);  // (x,y,"Text")
      u8g.drawStrP(83, 62, TextDELETE);  // (x,y,"Text")

      // Selection box:
      if(SelectorPosition == 1)
      {
        u8g.drawFrame(SelectBoxXPositionRight, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for START (x,y,width,height)
      }
      if(SelectorPosition == 0)
      {
        u8g.drawFrame(0, SelectBoxYPosition, SelectBoxWigth, SelectBoxHeight);  // Draw a square for NO (x,y,width,height)
      }
    }  // End of if page is on Display Options > Factory Reset





    // Display Options > Factory Reset 2:
    if(ScreenPage == 3311)  // If page is on Factory Reset 2
    {
      u8g.drawStrP(0, 8, TextFactoryresetin);  // (x,y,"Text")
      u8g.drawStrP(0, 18, Textprogress);  // (x,y,"Text")
      u8g.drawStrP(0, 38, TextPleasewait);  // (x,y,"Text")
    }  // End of if page is on Display Options > Factory Reset

  }  // End of display
  while(u8g.nextPage());  // End of the picture loop









  
  // If we are doing a Factory Reset, excecute this:
  if(ScreenPage == 3311)  // If page is on Factory Reset 2
  {
    // Clear all EEPROM:
    for(int i = 0 ; i < EEPROM.length() ; i++) {EEPROM.write(i, 255);}  // Go thourgh all the EEPROM addresses and write the default value (255)
    resetFunc();  // Do a soft Reset of Arduino 
  }
  
  
  






  
  // Print on serial monitor statistics for debugging:
  
  /*
  //Serial.print("\t");
  Serial.print("Slot:");
  Serial.print(CurrentProfileUserSlot);
  Serial.print("(");
  Serial.print(CurrentProfileEEPROMSector);
  Serial.print(")");
  */

  
/*
  //Serial.print("Raw: ");
  Serial.print(RawLoadCellReading);
  Serial.print("\t");
  Serial.print("Weight: ");
  Serial.print(HolderWeight);  // Holder weight not smoothed
  Serial.print("\t EmpSp: ");
  Serial.print(EmptySpoolMeasuredWeight);  // Holder weight not smoothed
  Serial.print("\t ProfW: ");
  Serial.print(CurrentProfileWeightShown);  // Holder weight not smoothed
  Serial.print("(");
  Serial.print(SmoothedWeight);  // Smoothed
  Serial.print(")");
  Serial.print("\t");
  Serial.print("DZ: ");
  Serial.print(WeightWithDeadzone);  // With deadzone
  //Serial.print("\t DZSamples: ");
  //Serial.print(DeadzoneSamples);  // With deadzone
  Serial.print("\t Out: ");
  Serial.print(FinalWeightToShow);  // With deadzone
  //Serial.print("\t DZSet: ");
  //Serial.print(DeadzoneAmountSet);  // With deadzone
*/

  
  //Serial.print("Button: ");
  //Serial.print(LeftButtonActivationStatePulses);
  //Serial.print(EnterButtonActivationStatePulses);
  //Serial.print(RightButtonActivationStatePulses);
  
  //Serial.print(ButtonDebouncingLastState);
  //Serial.print(ButtonDebouncingCounter);
  //Serial.print(RightButtonActivationStatePulses);
  
  
  
  /*
  // Serial EEPROM:
  Serial.print(" ");
  Serial.print(VariableCalibrationDone);
  Serial.print("\t");
  Serial.print(EmptyHolderRawValue);
  Serial.print("\t");
  Serial.print(TemporalFullSpoolRawValue);
  Serial.print(" ");
  Serial.print(FullSpoolRawValue);
  Serial.print("\t");
  Serial.print(TemporalFullSpoolWeight);
  Serial.print(" ");
  Serial.print(FullSpoolWeight);
  //Serial.print("\t");
  //Serial.print(EmptySpoolMeasuredWeight);
  // End of EEPROM Serial
  */
  
  /*
  Serial.print(CurrentProfileEEPROMSector);
  Serial.print("\t");
  Serial.print(CurrentProfileUserSlot);
  Serial.print("\t");
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[0]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[1]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[2]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[3]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[4]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[5]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[6]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[7]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[8]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[9]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[10]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[11]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[12]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[13]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[14]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[15]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[16]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[17]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[18]));
  Serial.print(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[19]));
  */
  
  
  /*
  // User Profile Slot and EEPROM sectors:
  Serial.print("\t");
  Serial.print(CurrentProfileUserSlot);
  Serial.print("(");
  Serial.print(CurrentProfileEEPROMSector);
  Serial.print(")\t");
  Serial.print(AmountOfCharProfileName);
  */

  
  /*
  Serial.print(UserProfileSlotSequence[0]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[1]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[2]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[3]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[4]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[5]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[6]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[7]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[8]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[9]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[10]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[11]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[12]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[13]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[14]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[15]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[16]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[17]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[18]);
  Serial.print("-");
  Serial.print(UserProfileSlotSequence[19]);
  */
  
  
  /*
  Serial.println();  // Final command to finish the line
  // The following is to print the if each EEPROM sector is available or not:
  // 1 = In use
  // 0 = Available
  int testToSerial;  // just temporal por serial
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[0]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print("0.");
  Serial.print(testToSerial);
  Serial.print("-1.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[1]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-2.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[2]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-3.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[3]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-4.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[4]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-5.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[5]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-6.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[6]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-7.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[7]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-8.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[8]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-9.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[9]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-10.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[10]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-11.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[11]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-12.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[12]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-13.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[13]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-14.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[14]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-15.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[15]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-16.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[16]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-17.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[17]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-18.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[18]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  Serial.print("-19.");
  if(EEPROM.read(EEPROM_AddressActiveOrDisabledProfile[19]) == 1){testToSerial=1;}else{testToSerial=0;}
  Serial.print(testToSerial);
  */
    
  
  //Serial.print("\t");
  //Serial.print(HolderWeight);
  //Serial.print("\t");
  //Serial.print(FinalWeightToShow);  // With deadzone


  //Serial.println();  // Final command to finish the line
  
}  // End of loop
