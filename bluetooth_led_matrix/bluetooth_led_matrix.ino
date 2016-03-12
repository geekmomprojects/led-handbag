#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_UART.h>

#include <FastLED.h>
#include "displayClass.h"

#define __DEBUG


// Teensy setup
#define HWSERIAL                    Serial2 // Means we use pins 9/10 on Teensy for TX/RX           
#define BLUEFRUIT_UART_MODE_PIN     12      // Teensy pin which controls Data/CMD mode

// Bluefruit settings
#define FACTORYRESET_ENABLE         0
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"
#define BUFSIZE                     256   // Size of the read buffer for incoming data
#define VERBOSE_MODE                true  // If set to 'true' enables debug output
#define BLE_READPACKET_TIMEOUT      500   // Timeout in ms waiting to read a response


// APA102 LED Strip settings for FastLED
#define DATA_PIN    4
#define CLOCK_PIN   5
#define CHIPSET     APA102
#define BRIGHTNESS  40

// Params for LED matrix width and height
const uint8_t kMatrixWidth = 10;
const uint8_t kMatrixHeight = 6;

#define NUM_LEDS (kMatrixWidth*kMatrixHeight)

// Use an extra matrix value as a safety pixel so we don't overwrite our 
//  array boundaries
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);

// Buffer for for creating smooth led animations or for scrolling
CRGB led_buffer_plus_safety_pixel[NUM_LEDS + 1];
CRGB* const led_buffer(led_buffer_plus_safety_pixel + 1);


// Class instances
DrawText        dText(leds, led_buffer, kMatrixWidth, kMatrixHeight);
DisplayRain     dRain(leds, led_buffer, kMatrixWidth, kMatrixHeight);
GameOfLife      dGame(leds, led_buffer, kMatrixWidth, kMatrixHeight);
BouncingPixels  dBounce(leds, led_buffer, kMatrixWidth, kMatrixHeight);
Twinkle         dTwinkle(leds, led_buffer, kMatrixWidth, kMatrixHeight);
Lines           dLines(leds, led_buffer, kMatrixWidth, kMatrixHeight);
Worm            dWorm(leds, led_buffer, kMatrixWidth, kMatrixHeight);

// Display modes
DisplayMatrix *autoDisplays[] = {&dRain, &dWorm, &dLines, &dTwinkle, &dGame, &dBounce};
const int numModes = sizeof(autoDisplays)/sizeof(autoDisplays[0]);
int displayMode = 0;

// Create Bluefruit object with Hardware Serial (Serial2 on Teensy).  Don't need
// RTS pin on Bluefruit, but make sure CTS pin is connected to ground
Adafruit_BluefruitLE_UART ble(HWSERIAL, BLUEFRUIT_UART_MODE_PIN);

// function prototypes over in packetparser.cpp
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

// the packet buffer
extern uint8_t packetbuffer[];

void setup() {
  Serial.begin(9600);
#ifdef __DEBUG
  Serial.println(F("Adafruit Bluefruit Command <-> LED Strip Programming"));
  Serial.println(F("--------------waiting 5 seconds------------"));
#endif
  delay(5000);

  /* Initialise the module */
#ifdef __DEBUG
  Serial.print(F("Initialising the Bluefruit LE module: "));
#endif
  if ( !ble.begin(VERBOSE_MODE) )
  {
#ifdef __DEBUG
    Serial.println(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
#endif
  }
#ifdef __DEBUG
  Serial.println( F("OK!") );
#endif
  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
//    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
#ifdef __DEBUG
      Serial.println(F("Couldn't factory reset"));
#endif
    }
  }
  /* Disable command echo from Bluefruit */
  ble.echo(false);

#ifdef __DEBUG
  Serial.println("Requesting Bluefruit info:");
#endif
  /* Print Bluefruit information */
  ble.info();

#ifdef __DEBUG
  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then Enter directions to send to Bluefruit"));
  Serial.println();
#endif
  ble.verbose(false);  // debug info is a little annoying after this point!


  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
#ifdef __DEBUG
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
#endif
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set module to DATA mode
#ifdef __DEBUG
  Serial.println( F("Switching to DATA mode!") );
#endif
  ble.setMode(BLUEFRUIT_MODE_DATA);



  // Set up LED stripts
  FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness( BRIGHTNESS );
  FastLED.clear();

  // Initialize random functions
  randomSeed(analogRead(0));
  
  dText.init();
  dText.addStringToBuffer("Hi", 3, 64);

  dRain.init();
  dGame.init();
  dBounce.init();
}



boolean getUartData() {
  boolean gotData = false;
  boolean modeChanged = false;
  String str = "";
  while (ble.available()) {
    gotData = true;
    int c = ble.read();
    str += (char)c;
#ifdef DEBUG   
    Serial.print((char) c);
#endif
    delay(10); // Give the rest of the data a chance to come in
  }
  if (gotData) {
    if (str[0] == '!') {
      str.toLowerCase();
      if (str == "!next") {       // choose next display mode
        displayMode = (displayMode + 1) % numModes;
        modeChanged = true;
      } else if (str == "!pal") { // Select next palette
        dText.nextPalette();
        autoDisplays[displayMode]->nextPalette();
      } 
    } else {
      //dText.init();
      // Be sure we don't have multiple twitter usernames conflated here - separate any usernames
      int strStart = 0;
      int findChar = str.indexOf('@', 1);
      while (findChar != -1) {
        String sub = str.substring(strStart, findChar);
        dText.addStringToBuffer(sub.c_str(), 3, random(255));
        strStart = findChar;
        findChar = str.indexOf('@', strStart+1);
      }
      String lastStr = str.substring(strStart);
      dText.addStringToBuffer(lastStr.c_str(), 3, random(255));
    }
#ifdef DEBUG
    Serial.println("");
#endif
  }
  return modeChanged;
}

void loop() {

  // Check for user input
  boolean modeChanged = false;

  // Check for data from the BLE
  modeChanged = getUartData();  
  // Update display
  // if (dataMode == DATA_SOURCE_UART && dText.displayingText()) {
  if (dText.displayingText()) {
    dText.update();
  } else {
    if (modeChanged) {
      autoDisplays[displayMode]->clearDisplay();
    }
    autoDisplays[displayMode]->update();
  }
}



