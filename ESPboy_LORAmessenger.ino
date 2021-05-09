 /*
LORA messenger for ESPboy LORA messenger module, by RomanS
ESPboy project: https://hackaday.io/project/164830-espboy-games-iot-stem-for-education-fun
*/

#include "lib/User_Setup.h"  //TFT_eSPI setup file for ESPboy
#define USER_SETUP_LOADED

#include <SoftwareSerial.h> //ESP software serial
#include "lib/ESPboy_keyboard.h"
#include "lib/ESPboy_keyboard.cpp"
#include "lib/glcdfont.c"
#include "lib/ESPboy_EBYTE.h"
#include "lib/ESPboy_EBYTE.cpp"
#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"
#include "lib/ESPboyTerminalGUI.h"
#include "lib/ESPboyTerminalGUI.cpp"


#define LORA_M0      13 //LORA MCP23017 PIN M0  [NORMAL MODE M1=0; M0=0;]
#define LORA_M1      14 //LORA MCP23017 PIN M1
#define LORA_AUX     15 //LORA MCP23017 PIN AUX
#define LORA_RX      D6
#define LORA_TX      D8

#define MAX_MESSAGE_STORE 50
#define ACK_MAX_SEND_ATTEMPTS 3
#define DELAY_ACK 100
#define ACK_TIMEOUT 2000

ESPboyInit myESPboy;
ESPboyTerminalGUI terminalGUIobj(&myESPboy.tft, &myESPboy.mcp);
keyboardModule keybModule(1,1,10000);
SoftwareSerial ESerial(LORA_RX, LORA_TX);
EBYTE lora (&ESerial, &myESPboy.mcp, LORA_M0, LORA_M1, LORA_AUX, 1000);


uint16_t messNo;
uint16_t packetNo = 0;
uint8_t keyboardCheck;

struct message{
  char messText[24];
  uint32_t messID; //ESP.getChipId();
  uint32_t messTimestamp; //pinMode(A0,INPUT); delay(random(digitalread(A0))); =millis();
  uint32_t messType; //0-mess / 1-ACK
  uint32_t hash; //should be last in struct
}*mess;


uint32_t calcCRC(message mess){
  uint8_t *hsh;
  uint32_t hash;
  hash = 0;
  hsh = (uint8_t *)&mess;
  for (uint16_t i=0; i<sizeof(mess)-sizeof(mess.messType) - sizeof(mess.hash); i++)
    hash += hsh[i]*(i+211);
  return (hash);
}



void sendPacket(){
  uint8_t gotACKflag;
  uint32_t waitACKtimeout;
  message messACK;

    myESPboy.playTone(200, 100);

    mess[messNo].messID = ESP.getChipId();
    mess[messNo].messTimestamp = millis();
    mess[messNo].messType = 0;
    String tempStr = terminalGUIobj.getTyping();
    strcpy (mess[messNo].messText, tempStr.c_str());
    
    mess[messNo].hash = calcCRC(mess[messNo]);

    gotACKflag = 0;
    terminalGUIobj.drawOwnTypingLine(F("Sending..."), TFT_BLUE);
      
    for (uint8_t i = 0; i < ACK_MAX_SEND_ATTEMPTS; i++){  
    lora.SendStruct(&mess[messNo], sizeof(mess[messNo]));  

      myESPboy.myLED.setRGB(0,0,10);
      
      waitACKtimeout = millis() + ACK_TIMEOUT;
      while (!lora.available() && waitACKtimeout > millis()) delay(300);
      if (lora.available()) {
      lora.GetStruct(&messACK, sizeof(messACK));
        
        //elliminate echo
        delay(200);
        lora.Clear();
        
        if (mess[messNo].hash == messACK.hash) gotACKflag = 1;
      }
      if (gotACKflag) break;
    }
    if (!gotACKflag) {
      terminalGUIobj.printConsole(mess[messNo].messText, TFT_DARKGREY, 0, 0);
      terminalGUIobj.drawOwnTypingLine(F("FAULT"), TFT_RED);
      myESPboy.myLED.setRGB(10,0,0);
    }
    else{
      terminalGUIobj.printConsole(mess[messNo].messText, TFT_YELLOW, 0, 0);
      terminalGUIobj.drawOwnTypingLine(F("OK"), TFT_GREEN);
      myESPboy.myLED.setRGB(0,10,0);
    }
    messNo++;
    if (messNo > MAX_MESSAGE_STORE-1) messNo = 0;
    packetNo++;

    delay(500);
    terminalGUIobj.drawOwnTypingLine(F(""), TFT_BLACK);
}



void recievePacket(){
  static uint32_t hash;
  
    myESPboy.myLED.setRGB(0,10,0);
    memset(&mess[messNo], sizeof(mess[messNo]),0);
    mess[messNo].hash = 65535;
    
    lora.GetStruct(&mess[messNo], sizeof(mess[messNo]));
    
    delay(100);
    lora.Clear();
    
    hash = calcCRC (mess[messNo]);
    
    if(hash == mess[messNo].hash){ 
      delay(DELAY_ACK);
      
      if(mess[messNo-1].hash != mess[messNo].hash){

        //sendACK
        mess[messNo].messType = 1;
        lora.SendStruct(&mess[messNo], sizeof(mess[messNo]));

        myESPboy.playTone(500, 200);
      
        terminalGUIobj.printConsole(mess[messNo].messText, TFT_MAGENTA, 0, 0);
      
        messNo++;
        if (messNo > MAX_MESSAGE_STORE-1) messNo = 0;
      }
      if(mess[messNo-1].hash == mess[messNo].hash && mess[messNo].messType != 1){
        mess[messNo].messType = 1;
        lora.SendStruct(&mess[messNo], sizeof(mess[messNo]));    
      }
    }
  else 
    myESPboy.myLED.setRGB(10,0,0);
}



void checkKeyboard(){
 static char keboardKey;
  keboardKey = keybModule.getPressedKey();
  if (keyboardCheck && keboardKey){
    String strTyping = terminalGUIobj.getTyping();
    if (keboardKey == '<' && strTyping.length())
      terminalGUIobj.setKeybParamTyping(strTyping.substring(0, strTyping.length()-1));
    if (keboardKey == '>' && strTyping.length()) 
      sendPacket();
    if (keboardKey != '>' && keboardKey != '<' && strTyping.length() < 30)
      terminalGUIobj.setKeybParamTyping(terminalGUIobj.getTyping() + (String)keboardKey);
  terminalGUIobj.drawTyping(0);
  }
}



void setup() {
  //Init ESPboy
  myESPboy.begin("LORA messenger v2.0");

 Serial.begin(9600);
 Serial.setRxBufferSize(2048);
 ESerial.begin(9600);
 

 mess = new message [MAX_MESSAGE_STORE];

 //reset random generator
 randomSeed(os_random());

 //draw interface
 terminalGUIobj.toggleDisplayMode(1);
 terminalGUIobj.printConsole(F("Init LORA..."), TFT_YELLOW, 0, 0);
 
//init LORA and restoring config
  ESerial.begin(9600);
  
  if (lora.init()) terminalGUIobj.printConsole(F("LORA OK"), TFT_GREEN, 0, 1);
  else {terminalGUIobj.printConsole(F("LORA FAULT"), TFT_RED, 0, 1);
    terminalGUIobj.printConsole(F("Connect LORA module"), TFT_RED, 0, 0);
    while(1)delay(1000);
  }

  //keyboard module init
 keyboardCheck = keybModule.begin();
 if (keyboardCheck) terminalGUIobj.printConsole(F("Keyboard found"), TFT_WHITE, 0 ,0);
 else terminalGUIobj.printConsole(F("Keyboard not found"), TFT_WHITE, 0 ,0);
 
  terminalGUIobj.printConsole("", TFT_BLACK, 0 ,0);
  
//LORA set parameters  
  lora.SetAirDataRate(ADR_2400);   // change the air data rate
  lora.SetAddressH(2);   // set the high address byte
  lora.SetAddressL(2);   // set the low address byte
  lora.SetChannel(7);     // set the channel (0-32 is pretty typical)
  lora.SetUARTBaudRate(UDR_9600);
  lora.SetTransmitPower(OPT_TP30);
  lora.SetMode(MODE_NORMAL);
  lora.SetFEC(FEC_ON);
  lora.SetPullup(PullUp_ON);
  lora.SetFixedMode(Transparent_Mode);
  lora.SetParityBit(PB_8N1);
  lora.SaveParameters(PERMANENT);  // save the parameters to the EBYTE EEPROM, you can save temp if periodic changes are needed

//draw interface
  terminalGUIobj.toggleDisplayMode(0);
}



void loop() {
  static uint32_t availableDelay = 0;
  
  if (terminalGUIobj.keysAction()) sendPacket();
  
  checkKeyboard();
  
  if (millis() > availableDelay+400){
    availableDelay = millis();
    if (lora.available()) recievePacket();
  }

  delay(200);  
  if (myESPboy.myLED.getRGB()) myESPboy.myLED.setRGB(0,0,0);
}
