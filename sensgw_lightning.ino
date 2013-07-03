/**
 * wlan slovenija sensor gateway
 * for sensgw v0.2 board and v0.5 board
 * for use with MPL115A2 I2C temperature and pressure sensor, controlls an LED
 * Instructions
 * see http://dev.wlan-si.net/wiki/Telemetry/sensgw
 * Author: Luka Mustafa - Musti (musti@wlan-si.net)
 * Revision history
 * 2.3.2013 - version 0.1 released
 */

// The following libraries are required for communication and sensors
// Include libraries required for your sensors

// I2C library used is not standard Wire, but an improved version of it
//http://dsscircuits.com/articles/arduino-i2c-master-library.html
#include "I2C.h"
// System variables
#define hwversion 0.5 // this must equal the version printed on the board
#define fwversion 0.26 // this must equal to the version of this code

//***************************************************************
// Definitions of variables and objects for sensors


//memory structure
// 1k available

//time 16bit; type 2bit ; distance 6bit; //power 16bit;
// 1k available 24bit per entry, 50 entries max

typedef struct
  {
      uint16_t time;
      uint8_t  typedist;
  }  lightning_log;

lightning_log lightning[56];

//100ms resolution -> 10 per second, 600 per minute, 100min wrap around

#include <AS3935.h>

void printAS3935Registers();

// Iterrupt handler for AS3935 irqs
// and flag variable that indicates interrupt has been triggered
// Variables that get changed in interrupt routines need to be declared volatile
// otherwise compiler can optimize them away, assuming they never get changed
void AS3935Irq();
volatile int AS3935IrqTriggered;

// Library object initialization First argument is interrupt pin, second is device I2C address
AS3935 AS3935(2,0);

//***************************************************************
// System variables - DO NOT EDIT
// UART variables
char RxBuffer[20]; //RX buffer
char data; // storing intermediate chars
byte index = 0; // buffer index

//for loop variable
char i=0;

//for lightning
int detectionCount;
int calibOff;


//other variables
int ledstat=0;
boolean i2cpwr=0; //I2c power transistor PNP control, low=ON

//***************************************************************
// Pointers to functions called via serial port

// Command "ACOM /x calls" GETfunctions array
// ACOM /0 must always be (int)testGET, where x is a positive integer

// Command "ACOM /x /v" calls SETfunctions array, where v is an integer
// ACOM /0 v must always be (int)testSET

int GETfunctions[6]={(int)testGET,(int)getTime,(int)getDetection,(int)getCalibOffset,(int)getNoise,(int)resetSystem};
int SETfunctions[4]={(int)testSET,(int)setLED,(int)setGain,(int)setDisturber};

/*
ACOM /0 - test
ACOM /1 - time in 100ms
ACOM /2 - detection results list, entries separated by comma, each entry <time> <type> <distance> <power>
ACOM /3 - calibartion offset
ACOM /4 - noise level
ACOM /5 - reset system

ACOM /0 x - test set
ACOM /1 x - led on/off
ACOM /2 x - set gain 0-15
ACOM /3 x - set disturber 1 enable(default), 0 disable


*/


//***************************************************************
// Functions for reading/writing and sensor communication

//**************************
// GET: Lightning Strokes
void getDetection(){
  for(char i=0;i<detectionCount;i++){ 
    Serial.print(lightning[i].time);
    Serial.print(" ");
    Serial.print(lightning[i].typedist >> 6);
    Serial.print(" ");
    Serial.print(lightning[i].typedist&0x1F);
    Serial.print(" ");
    Serial.print(0); // no power for now
    Serial.print(",");
  }
  detectionCount=0;
  Serial.print("\n\n"); //terminator
}

//************************** 
// GET: Lightning Strokes
void getCalibOffset(){
  Serial.print(calibOff);
  Serial.print("\n\n"); //terminator
}

//**************************
// GET: Lightning Strokes
void getNoise(){
  Serial.print(AS3935.getNoiseFloor());
  Serial.print("\n\n"); //terminator
}

//**************************
// GET: Lightning Strokes
void getTime(){
  Serial.print((uint16_t)(millis()/100));
  Serial.print("\n\n"); //terminator
}

//**************************
// SET: Control LED
void setLED(int data){
  digitalWrite(5, data);
  Serial.print("OK"); //ever SET function must return something
  Serial.print("\n\n"); //terminator
}

//**************************
// GET: Control LED
void resetSystem(){
  Serial.print("OK"); //ever SET function must return something
  digitalWrite(4, HIGH); //OFF
  delay(500);
  setup();
  
}

//**************************
// SET: gain
void setGain(int data){
  AS3935.setGain((char)(data&0x1F)); //limit max value
  Serial.print("OK"); //ever SET function must return something
  Serial.print("\n\n"); //terminator
}

//**************************
// SET: disturber
void setDisturber(int data){
  if(data){
    AS3935.enableDisturbers();
  }
  else{
    AS3935.disableDisturbers();
  }
  Serial.print("OK"); //ever SET function must return something
}

//****************************************************************
// Initialization sequence, edit ONLY the specified part
void setup() {
    //initialize UART
    Serial.begin(115200);
    
    //initialize I2C
    I2c.begin();
    I2c.pullup(true);
    I2c.setSpeed(1); //400kHz
    //initialize I2C power transistor
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    
    //**************************
    // EDIT from here!!!
    //setup pins
    pinMode(5, OUTPUT); //LED
    
    // reset all internal register values to defaults
    AS3935.reset();
    // and run calibration
    // if lightning detector can not tune tank circuit to required tolerance,
    // calibration function will return false
    calibOff=AS3935.calibrate();
  
    // first let's turn on disturber indication and print some register values from AS3935
    // tell AS3935 we are indoors, for outdoors use setOutdoors() function
    AS3935.setOutdoors();
    AS3935.enableDisturbers();
    //printAS3935Registers();
    AS3935IrqTriggered = 0;
    attachInterrupt(0,AS3935Irq,RISING);
    detectionCount=0;
}



//****************************************************************
// do not edit from here on, unless you know what your are doing

// tests the communication and returns version
void testGET(){
  Serial.print("sensgw hw ");
  Serial.print(hwversion);
  Serial.print(" fw ");
  Serial.print(fwversion);
  Serial.print("\n\n"); //terminator  
  
 // printAS3935Registers();
}

void testSET(int data){
  Serial.print("test write, decimal value: ");
  Serial.print(data);
  Serial.print("\n\n"); //terminator
}

void loop()
{
  if(AS3935IrqTriggered)
  {
     ledstat=!ledstat;
     digitalWrite(5, ledstat);
     char type;
     
    // reset the flag
    AS3935IrqTriggered = 0;
    // first step is to find out what caused interrupt
    // as soon as we read interrupt cause register, irq pin goes low
    int irqSource = AS3935.interruptSource();
    // returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!
    if (irqSource & 0b0001){
      //Serial.println("Noise level too high, try adjusting noise floor");
      type=0;
    }
    if (irqSource & 0b0100){
      type=1;
    }
    if (irqSource & 0b1000)
    {
      // need to find how far that lightning stroke, function returns approximate distance in kilometers,
      // where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
      // everything in between is just distance in kilometers
      type=2;
    }   
    lightning[detectionCount%56].typedist=(type<<6)|(AS3935.lightningDistanceKm()&0x3F);
    lightning[detectionCount++].time=(uint16_t)(millis()/100);
    
    //getDetection();
  }
  
  // handles incoming UART commands
  // wait for at least 7 characters:
  while (Serial.available() > 7) {
    int x = 0;
    int v = 0;
    // Check for "ACOM /" string
    if(Serial.find("ACOM /")){
      // parse command number after /
      x = Serial.parseInt(); 
      //if termianted, call GET
      if (Serial.read() == '\n'){
        // call GET function
        ((void (*)()) GETfunctions[x])();
      }
      //if nto terminated, read value argument and call SET
      else{
        v = Serial.parseInt();
        ((void (*)(int)) SETfunctions[x])(v); 
      }
      //more arguments can be supported in the same manner
    }
  }
}

void printAS3935Registers()
{
  int noiseFloor = AS3935.getNoiseFloor();
  int spikeRejection = AS3935.getSpikeRejection();
  int watchdogThreshold = AS3935.getWatchdogThreshold();
  Serial.print("Noise floor is: ");
  Serial.println(noiseFloor,DEC);
  Serial.print("Spike rejection is: ");
  Serial.println(spikeRejection,DEC);
  Serial.print("Watchdog threshold is: ");
  Serial.println(watchdogThreshold,DEC);  
}

// this is irq handler for AS3935 interrupts, has to return void and take no arguments
// always make code in interrupt handlers fast and short
void AS3935Irq()
{
  AS3935IrqTriggered = 1;
}
