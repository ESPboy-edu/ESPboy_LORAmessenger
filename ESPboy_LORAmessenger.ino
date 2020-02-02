/*
LORA messenger for ESPboy LORA messenger module, by RomanS
ESPboy project: https://hackaday.io/project/164830-espboy-games-iot-stem-for-education-fun
*/


#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "Adafruit_MCP23017.h"
#include "Adafruit_MCP4725.h"
#include "TFT_eSPI.h"
#include "ESPboy_LED.h"
#include "lib/glcdfont.c"
#include "lib/ESPboyLogo.h"
#include "ESPboy_EBYTE.h"

#define LHSWAP(w)       (((w)>>8)|((w)<<8))
#define MCP23017address 0 // actually it's 0x20 but in <Adafruit_MCP23017.h> lib there is (x|0x20) :)
#define MCP4725address  0x60 //DAC driving LCD backlit
#define LEDPIN          D4
#define SOUNDPIN        D3
#define CSTFTPIN        8 //CS MCP23017 PIN to TFT

#define LORA_M0      13 //LORA MCP23017 PIN M0  [NORMAL MODE M1=0; M0=0;]
#define LORA_M1      14 //LORA MCP23017 PIN M1
#define LORA_AUX     15 //LORA MCP23017 PIN AUX
#define LORA_RX      D6
#define LORA_TX      D8

#define MIN_DELAY_BETWEEN_PACKETS 5000
#define MAX_DELAY_BETWEEN_PACKETS 10000
#define MAX_MESSAGE_STORE 100
#define MAX_CONSOLE_STRINGS 10
#define CURSOR_BLINKING_PERIOD 1000

#define PAD_LEFT        0x01
#define PAD_UP          0x02
#define PAD_DOWN        0x04
#define PAD_RIGHT       0x08
#define PAD_ACT         0x10
#define PAD_ESC         0x20
#define PAD_LFT         0x40
#define PAD_RGT         0x80
#define PAD_ANY         0xff

Adafruit_MCP23017 mcp;
Adafruit_MCP4725 dac;
ESPboyLED myled;

TFT_eSPI tft = TFT_eSPI();
SoftwareSerial ESerial(LORA_RX, LORA_TX);
EBYTE lora (&ESerial, &mcp, LORA_M0, LORA_M1, LORA_AUX, 1000);


constexpr uint8_t keybOnscr[3][21] PROGMEM = {
"1234567890ABCDEFGHIJ",
"KLMNOPQRSTUVWXYZ_+-=",
"?!@#$%^&*()_[]:.,'<E",
};


static String consolestrings[MAX_CONSOLE_STRINGS+1];
static uint16_t consolestringscolor[MAX_CONSOLE_STRINGS+1];
uint16_t line_buffer[46];
uint16_t packetNo = 0;
uint8_t keyState = 0, selX = 0, selY = 0;

String typing = "";
uint32_t cursorBlinkMillis = 0;
uint8_t cursorTypeFlag = 0;
char cursorType[2]={220,'_'};
uint8_t sendFlag = 0;


#pragma pack(push, 1)
struct message{
  char messText[24];
  uint32_t messID; //ESP.getChipId();
  uint32_t messTimestamp; //pinMode(A0,INPUT); delay(random(digitalread(A0))); =millis();
  uint8_t messType;
  uint32_t hash;
};
#pragma pack(pop)


static message mess[MAX_MESSAGE_STORE];
static uint16_t messNo = 1;

void drawConsole(String bfrstr, uint16_t color){
  for (int i=0; i<MAX_CONSOLE_STRINGS; i++) {
    consolestrings[i] = consolestrings[i+1];
    consolestringscolor[i] = consolestringscolor[i+1];
  }
  consolestrings[MAX_CONSOLE_STRINGS-1] = bfrstr;
  consolestringscolor[MAX_CONSOLE_STRINGS-1] = color;
  tft.fillRect(1,1,126,MAX_CONSOLE_STRINGS*8,TFT_BLACK);
  for (int i=0; i<MAX_CONSOLE_STRINGS; i++)
    printFast(4, i*8+2, consolestrings[i], consolestringscolor[i], TFT_BLACK);
}



void drawCharFast(uint16_t x, uint16_t y, uint8_t c, uint16_t color, uint16_t bg){
 uint16_t i, j, c16, line;
  for (i = 0; i < 5; ++i){
    line = pgm_read_byte(&font[c * 5 + i]);
    for (j = 0; j < 8; ++j){
      c16 = (line & 1) ? color : bg;
      line_buffer[j * 5 + i] = LHSWAP(c16);
      line >>= 1;
    }
  }
  tft.pushImage(x, y, 5, 8, line_buffer);
}


void printFast(int x, int y, String str, int16_t color, uint16_t bg){
 char c, i=0;
  while (1){
    c =  str[i++];
    if (!c) break;
    drawCharFast(x, y, c, color, bg);
    x += 6;
  }
}


void printLORAparameters(){
  Serial.println();
  Serial.print(F("Model no.               : "));  Serial.println(lora._Model, HEX);
  Serial.print(F("Version                 : "));  Serial.println(lora._Version, HEX);
  Serial.print(F("Features                : "));  Serial.println(lora._Features, HEX);
  Serial.print(F("Mode (HEX)              : "));  Serial.println(lora._Save, HEX);
  Serial.print(F("AddH (HEX)              : "));  Serial.println(lora._AddressHigh, HEX); 
  Serial.print(F("AddL (HEX)              : "));  Serial.println(lora._AddressLow, HEX);;
  Serial.print(F("Sped (HEX)              : "));  Serial.println(lora._Speed, HEX); 
  Serial.print(F("Chan (HEX)              : "));  Serial.println(lora._Channel, HEX); 
  Serial.print(F("Optn (HEX)              : "));  Serial.println(lora._Options, HEX);  
  Serial.print(F("SpeedParityBit (HEX)    : "));  Serial.println(lora._ParityBit, HEX); 
  Serial.print(F("SpeedUARTDataRate (HEX) : "));  Serial.println(lora._UARTDataRate, HEX); 
  Serial.print(F("SpeedAirDataRate (HEX)  : "));  Serial.println(lora._AirDataRate, HEX);
  Serial.print(F("OptionTrans (HEX)       : "));  Serial.println(lora._OptionTrans, HEX); 
  Serial.print(F("OptionPullup (HEX)      : "));  Serial.println(lora._OptionPullup, HEX); 
  Serial.print(F("OptionWakeup (HEX)      : "));  Serial.println(lora._OptionWakeup, HEX); 
  Serial.print(F("OptionFEC (HEX)         : "));  Serial.println(lora._OptionFEC, HEX); 
  Serial.print(F("OptionPower (HEX)       : "));  Serial.println(lora._OptionPower, HEX); 
  Serial.println();
}


int checkKey(){
  keyState = ~mcp.readGPIOAB() & 255;
  return (keyState);
}


void redrawOnscreen(uint8_t slX, uint8_t slY){
  tft.fillRect(0, 128 - 24, 128, 24, TFT_BLACK);
  for (uint8_t i=0; i<20; i++) drawCharFast(i*6+4, 128-24-2, pgm_read_byte(&keybOnscr[0][i]), TFT_YELLOW, TFT_BLACK); 
  for (uint8_t i=0; i<20; i++) drawCharFast(i*6+4, 128-16-2, pgm_read_byte(&keybOnscr[1][i]), TFT_YELLOW, TFT_BLACK); 
  for (uint8_t i=0; i<20; i++) drawCharFast(i*6+4, 128-8-2, pgm_read_byte(&keybOnscr[2][i]), TFT_YELLOW, TFT_BLACK); 
  drawCharFast(slX*6+4, 128-24+slY*8-2, pgm_read_byte(&keybOnscr[slY][slX]), TFT_RED, TFT_BLACK); 
  drawCharFast(6*19+4, 128-24+2*8-2, 'E', TFT_WHITE, TFT_BLACK); 
  drawCharFast(6*18+4, 128-24+2*8-2, '<', TFT_WHITE, TFT_BLACK); 
}

void redrawSelected (uint8_t slX, uint8_t slY){
 static uint8_t prevX = 0, prevY = 0;
  drawCharFast(prevX*6+4, 128-24+prevY*8-2, pgm_read_byte(&keybOnscr[prevY][prevX]), TFT_YELLOW, TFT_BLACK); 
  drawCharFast(6*19+4, 128-24+2*8-2, 'E', TFT_WHITE, TFT_BLACK); 
  drawCharFast(6*18+4, 128-24+2*8-2, '<', TFT_WHITE, TFT_BLACK);
  drawCharFast(slX*6+4, 128-24+slY*8-2, pgm_read_byte(&keybOnscr[slY][slX]), TFT_RED, TFT_BLACK); 
  prevX = slX; 
  prevY = slY; 
}


void drawTyping(){
    tft.fillRect(1, 128-5*8, 126, 8, TFT_BLACK);  
    printFast(4, 128-5*8, typing+cursorType[cursorTypeFlag], TFT_WHITE, TFT_BLACK);
}


void drawBlinkingCursor(){
   if (millis() > cursorBlinkMillis+CURSOR_BLINKING_PERIOD){
      cursorBlinkMillis = millis();
      cursorTypeFlag = !cursorTypeFlag;
      drawTyping();
   }
}


void keybOnscreen(){
   if (checkKey()){
      if ((keyState&PAD_RIGHT) && selX < 19) { selX++; redrawSelected (selX, selY); }
      if ((keyState&PAD_LEFT) && selX > 0) { selX--; redrawSelected (selX, selY); }
      if ((keyState&PAD_DOWN) && selY < 2) { selY++; redrawSelected (selX, selY); }
      if ((keyState&PAD_UP) && selY > 0) { selY--; redrawSelected (selX, selY); }
      
      if ((((keyState&PAD_ACT) && (selX == 19 && selY == 2)) || (keyState&PAD_ACT && keyState&PAD_ESC) || (keyState&PAD_RGT)) && typing.length()>0){
        sendFlag = 1;
        tft.fillRect(1, 128-5*8, 126, 8, TFT_BLACK);  
        printFast(4, 128-5*8, "Sending...", TFT_RED, TFT_BLACK); 
        delay(100);
        } //enter
      else
        if (((keyState&PAD_ACT) && (selX == 18 && selY == 2) || (keyState&PAD_ESC)) && typing.length()>0){//back space
            typing.remove(typing.length()-1); 
            drawTyping();
            delay(200);
        } 
        else
          if ((keyState&PAD_ACT) && (selX == 16 && selY == 1) && typing.length() < 19){//SPACE
            typing += " "; 
            drawTyping();
            delay(200);
          } 
          else
            if (keyState&PAD_ACT && typing.length() < 19) {
            typing += (char)pgm_read_byte(&keybOnscr[selY][selX]); drawTyping();
            delay(200);
            }
   }
  drawBlinkingCursor();
}


void setup() {
  Serial.begin(9600);
  ESerial.begin(9600);
  
  WiFi.mode(WIFI_OFF);
  delay(100);

//LED init
  myled.begin();

//DAC init, LCD backlit off
  dac.begin(MCP4725address);
  delay(50);
  dac.setVoltage(0, false);
  delay(100);

//MCP23017 and buttons init, should preceed the TFT init
  mcp.begin(MCP23017address);
  delay(100);

  for (int i = 0; i < 8; ++i){
    mcp.pinMode(i, INPUT);
    mcp.pullUp(i, HIGH);
  }

//Sound init and test
  pinMode(SOUNDPIN, OUTPUT);
  tone(SOUNDPIN, 200, 100);
  delay(100);
  tone(SOUNDPIN, 100, 100);
  delay(100);
  noTone(SOUNDPIN);

//TFT init
  mcp.pinMode(CSTFTPIN, OUTPUT);
  mcp.digitalWrite(CSTFTPIN, LOW);
  delay(200);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

//draw ESPboylogo
  tft.drawXBitmap(30, 20, ESPboyLogo, 68, 64, TFT_YELLOW);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(22, 95);
  tft.print(F("LORA messenger"));

//LCD backlit on
  for (uint8_t bcklt=0; bcklt<100; bcklt++){
    dac.setVoltage(bcklt*20, false);
    delay(10);}
  dac.setVoltage(4095, true);

 //reset random generator
 pinMode(A0,INPUT); 
 delay(random(map(digitalRead(A0),0,1024,0,10))*1000+1000); 

 //clear screen
 tft.fillScreen(TFT_BLACK);
 delay(200);
 
 //draw interface
 drawConsole(F("Init LORA..."), TFT_WHITE);
 tft.fillRect(0, 128-24, 128, 24, TFT_BLACK);
 redrawOnscreen(selX, selY);
 drawTyping();
 tft.drawRect(0, 128-3*8-5, 128, 3*8+5, TFT_NAVY);
 tft.drawRect(0, 0, 128, MAX_CONSOLE_STRINGS*8+4, TFT_NAVY);
 tft.drawRect(0, 0, 128, 128, TFT_NAVY);

//init LORA and restoring config
  ESerial.begin(9600);
  
  if (lora.init()) drawConsole("OK", TFT_WHITE);
  else drawConsole("FAULT", TFT_RED);
  
//  lora.ReadParameters();
//  printLORAparameters();
  
  lora.SetAirDataRate(ADR_2400);   // change the air data rate
  lora.SetAddressH(2);   // set the high address byte
  lora.SetAddressL(2);   // set the low address byte
  lora.SetChannel(17);     // set the channel (0-32 is pretty typical)
  lora.SetUARTBaudRate(UDR_9600);
  lora.SetTransmitPower(OPT_TP30);
  lora.SetMode(MODE_NORMAL);
  lora.SetFEC(FEC_ON);
  lora.SetPullup(PullUp_ON);
  lora.SetFixedMode(Transparent_Mode);
  lora.SetParityBit(PB_8N1);
  
  lora.SaveParameters(PERMANENT);  // save the parameters to the EBYTE EEPROM, you can save temp if periodic changes are needed
 
//  lora.ReadParameters();
//  printLORAparameters();
}


void sendPacket(){
  static uint8_t *hsh;
  static uint32_t hash;

    tone(SOUNDPIN,200, 100);
    
    mess[messNo].messID = ESP.getChipId();
    mess[messNo].messTimestamp = millis();
    mess[messNo].messType = messNo;
    strcpy (mess[messNo].messText, typing.c_str());

    mess[messNo].hash = 0;
    hsh = (uint8_t *)&mess[messNo];
    for (uint8_t i=0; i<sizeof(mess[messNo])-sizeof(mess[messNo].hash); i++) 
      mess[messNo].hash += hsh[i];

    lora.SendStruct(&mess[messNo], sizeof(mess[messNo]));

    drawConsole((String)messNo + ":" + mess[messNo].messText, TFT_YELLOW);
    
    sendFlag = 0;
    typing = "";  
    messNo++;
    if (messNo > MAX_MESSAGE_STORE-1) messNo = 0;
    packetNo++;
}


void recievePacket(){
  static uint8_t *hsh;
  static uint32_t hash;

    myled.setRGB(0,10,0);
    memset(&mess[messNo], sizeof(mess[messNo]),0);
    mess[messNo].hash = 65535;
    
    lora.GetStruct(&mess[messNo], sizeof(mess[messNo]));
    
    hash = 0;
    hsh = (uint8_t *)&mess[messNo];
    for (uint8_t i=0; i<sizeof(mess[messNo])-sizeof(mess[messNo].hash); i++) 
      hash += hsh[i];
      
    if(hash == mess[messNo].hash){
      tone(SOUNDPIN, 500, 200);
      drawConsole((String)messNo + ":" + mess[messNo].messText, TFT_MAGENTA);
      
      messNo++;
      if (messNo > MAX_MESSAGE_STORE-1) messNo = 0;
    }
  else 
    myled.setRGB(10,0,0);

//elliminate echo
  delay(100);
  lora.Clear();
}



void loop() {
  static uint32_t availableDelay = 0;
  
  if (sendFlag) sendPacket();

  if (millis() > availableDelay+500){
    availableDelay = millis();
    if (lora.available()) recievePacket();
  }
  delay(150);
  if (myled.getRGB()) myled.setRGB(0,0,0);
  keybOnscreen();
}
