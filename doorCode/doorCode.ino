#include <Servo.h>

/*
   HID RFID Reader Wiegand Interface for Arduino Uno
   Written by Daniel Smith, 2012.01.30
   www.pagemac.com

   This program will decode the wiegand data from a HID RFID Reader (or, theoretically,
   any other device that outputs weigand data).
   The Wiegand interface has two data lines, DATA0 and DATA1.  These lines are normall held
   high at 5V.  When a 0 is sent, DATA0 drops to 0V for a few us.  When a 1 is sent, DATA1 drops
   to 0V for a few us.  There is usually a few ms between the pulses.

   Your reader should have at least 4 connections (some readers have more).  Connect the Red wire
   to 5V.  Connect the black to ground.  Connect the green wire (DATA0) to Digital Pin 2 (INT0).
   Connect the white wire (DATA1) to Digital Pin 3 (INT1).  That's it!

   Operation is simple - each of the data lines are connected to hardware interrupt lines.  When
   one drops low, an interrupt routine is called and some bits are flipped.  After some time
   of not receiving any bits, the Arduino will decode the data.  I've only added the 26 bit and
   35 bit formats, but you can easily add more.

*/
/*
   Door code by Xavier Jiang
   (some cards and a few comments added by Hayden Fuller)
   Previous work done by Hayden, Xavier, Gavin, Alek, and Joe
   
   For use with any HID card reader and any door knob connected to a servo motor
   If adding new members, look for the following line
   //COMMENT THIS OUT, SERIAL MAKES IT HANG
   and uncomment it to get serial out so you can add them at
   //ADD CODES HERE
   and update the number above them
   //CHANGE THIS NUMBER
   then comment those first two line back out when you're done, because again, SERIAL MAKES IT HANG!
*/

Servo myservo;  // create servo object to control a servo
//Maybe constexpr?
#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  

constexpr uint8_t open_angle = 120;

uint8_t databits[MAX_BITS];    // stores all of the data bits
uint8_t bitCount;              // number of bits currently captured
uint8_t flagDone;              // goes low when data is currently being captured
uint16_t weigand_counter;        // countdown until we assume there are no more bits
//facilityCode is never actually used?
uint16_t facilityCode = 0;      // decoded facility code
uint32_t cardCode = 0;          // decoded card code
int16_t pos = 0;

//int16_t speed = 1;

bool accept_data = false;

uint8_t i;

constexpr uint8_t AccessListSize = 9; //CHANGE THIS NUMBER
//By default you can only use “sizeof”, which isnt even fast
//at least if array size is incorrect the compiler will throw a error
//so you WILL need to fix it
static const PROGMEM uint32_t AccessList[AccessListSize] = { //ADD CODES HERE
  950212, // ??
  947924, // Hayden
  948560, // ??
  947121, // Xavier
  943297, //Scarf
  950912, //Kelsey
  943634, //Alec
  946096, //Anthony
  941159 //Joe Z
};

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0(void)
{
  if (accept_data) {
    ////Serial.print("0");   // uncomment this line to display raw binary
    if (bitCount < MAX_BITS)
      bitCount++;
    flagDone = 0;
    weigand_counter = WEIGAND_WAIT_TIME;
  }
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1(void)
{

  if (accept_data) {
    ////Serial.print("1");   // uncomment this line to display raw binary

    databits[bitCount] = 1;
    if (bitCount < MAX_BITS)
      bitCount++;
    flagDone = 0;
    weigand_counter = WEIGAND_WAIT_TIME;
  }
}

bool AccessCheck(uint32_t incoming)
{
  //if(!(incoming==0)){Serial.println(incoming);} //COMMENT THIS OUT, SERIAL MAKES IT HANG
  for (uint8_t it = 0; it < AccessListSize; ++it)
    if ((uint32_t)(pgm_read_dword_near(AccessList + it) == incoming))
      return true;
  return false;
}

void setup(void)
{
  //pinMode(13, OUTPUT);  // LED
  pinMode(2, INPUT);     // DATA0 (INT0)
  pinMode(3, INPUT);     // DATA1 (INT1)
  //pinMode(8, OUTPUT);

 
  // binds the ISR functions to the falling edge of INTO and INT1

  
  accept_data = false;

  weigand_counter = WEIGAND_WAIT_TIME;

  //Serial.begin(9600); //COMMENT THIS OUT, SERIAL MAKES IT HANG
  //Serial.println("RFID Readers");

  myservo.attach(12);  // attaches the servo on pin 12 to the servo object

  //for re-setting servo position; likely only needed 1 command/loop to write servo position to 0
delay(100);
  myservo.write(open_angle);              // tell servo to go to position in variable 'pos'
  delay(1500);
  myservo.write(0);              // tell servo to go to position in variable 'pos'

  //for (pos = 0; pos < 130; pos += 1) { // goes to 130 degrees
  // in steps of 1 degree
  //  myservo.write(pos);              // tell servo to go to position in variable 'pos'
  //  delay(10);                       // wait for the servo
  //}
  //for (pos = 130; pos > 0; pos -= 1) { // return to 0 degrees
  //  myservo.write(pos);
  // tell servo to go to position in variable 'pos'
  //  delay(10);                       // waits 15ms for the servo to reach the position
  //}


  // W   W    A   RRRRR N   N  I  N   N  GGGG   !
  // W   W   A A  R   R NN  N  I  NN  N G       !
  // W W W   A A  RRRRR N N N  I  N N N G  GG   !
  // W W W  AAAAA R  R  N  NN  I  N  NN G   G   !
  //  W W   A   A R   R N   N  I  N   N  GGG    !
  //
  //the following work becase of the following:
  //
  //1. the arduino uno have only two interrupts
  //
  //2. the first interrupt happen to correspond to pin 0, and
  // the second interrupt happen to correspond to pin 1
    attachInterrupt(0, ISR_INT0, FALLING);
    attachInterrupt(1, ISR_INT1, FALLING);
  //equivalent:
  //attachInterrupt(digitalPinToInterrupt(2), ISR_INT0, FALLING);
  //attachInterrupt(digitalPinToInterrupt(3), ISR_INT1, FALLING);
 accept_data = true;
}

void loop(void)
{
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;
  }

  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    detachInterrupt(0);
    detachInterrupt(1);
    accept_data = false;
    //semi-global counter variable? What is this?
    //Serial.print("Read ");
    //Serial.print(bitCount);
    //Serial.print(" bits. ");

    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info

    if (bitCount == 35) {
      // 35 bit HID Corporate 1000 format
      // facility code = bits 2 to 14 ??????
      for (i = 2; i < 14; i++) {
        facilityCode <<= 1;
        facilityCode |= databits[i];
      }
      // card code = bits 15 to 34
      for (i = 14; i < 34; i++) {
        cardCode <<= 1;
        cardCode |= databits[i];
      }

      //printBits();
    }
    //else
    //Serial.println("not 35 bit");

    //I don't think we need this?

    /*
      else/*//*/ if (bitCount == 26) {
      // standard 26 bit format
      // facility code = bits 2 to 9 ??????
      for (i=1; i<9; i++) {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }

      // card code = bits 10 to 23
      for (i=9; i<25; i++) {
         cardCode <<=1;
         cardCode |= databits[i];
      }
    }
    else
      // you can add other formats if you want!
     //Serial.println("not 26 bit");
*/

    // Servo control and binary list
    //if(cardCode == 950212 || cardCode == 947924 || cardCode == 948560 || cardCode == 947121)
    //              ??                    Hayden                ??                    Xavier
    if (AccessCheck(cardCode)) {
      //Serial.println("open");
      myservo.write(open_angle);
      delay(2000);
      myservo.write(0);
      /*for (pos = 0; pos < 130; pos += 1) { // goes to 130 degrees
        // in steps of 1 degree
        myservo.write(pos);              // tell servo to go to position in variable 'pos'
        //Serial.println(pos);
        delay(10);                       // wait for the servo
        }
        delay(750);
        for (pos = 130; pos > 0; pos -= 1) { // return to 0 degrees
        myservo.write(pos);
        // tell servo to go to position in variable 'pos'
        //Serial.println(pos);
        delay(10);                       // waits for the servo
        }*/

    }
    printBits();

    // cleanup and get ready for the next card
    bitCount = 0;
    facilityCode = 0;
    cardCode = 0;
    for (i = 0; i < MAX_BITS; i++)
      databits[i] = 0;
    attachInterrupt(0, ISR_INT0, FALLING);
  attachInterrupt(1, ISR_INT1, FALLING);
  accept_data = true;
  }
}


void printBits(void)
{
  //uint8_t i;

  //Serial.print("FC = ");
  //Serial.print(facilityCode);
  //Serial.print(", CC = ");
  //Serial.println(cardCode);
  //for (i = 8; i < 34; i++)
  //Serial.print(databits[i]);

  //Serial.println();
}
