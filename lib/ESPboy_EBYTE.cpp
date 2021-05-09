/*
ESPboy LORA EBYTE library specially adopted and simplified for ESPboy project by RomanS
https://hackaday.io/project/164830-espboy-games-iot-stem-for-education-fun
*/


/*
  The MIT License (MIT)
  Copyright (c) 2019 Kris Kasrpzak
  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  On a personal note, if you develop an application or product using this library 
  and make millions of dollars, I'm happy for you!
*/

/* 
  Code by Kris Kasprzak kris.kasprzak@yahoo.com
  This library is intended to be used with EBYTE transcievers, small wireless units for MCU's such as
  Teensy and Arduino. This library let's users program the operating parameters and both send and recieve data.
  This company makes several modules with different capabilities, but most #defines here should be compatible with them
  All constants were extracted from several data sheets and listed in binary as that's how the data sheet represented each setting
  Hopefully, any changes or additions to constants can be a matter of copying the data sheet constants directly into these #defines
  Usage of this library consumes around 970 bytes
  Revision		Data		Author			Description
  1.0			3/6/2019	Kasprzak		Initial creation
  2.0			10/21/2019	Kasprzak		modified code to allow no MO or M1 for use with limited wires
   
*/


#include <Stream.h>
#include "Adafruit_MCP23017.h"
#include "Arduino.h"




//create the transciever object
EBYTE::EBYTE(Stream *s, Adafruit_MCP23017 *mcp, int8_t PIN_M0, int8_t PIN_M1, int8_t PIN_AUX, uint32_t ReadTimeout ){
	_s = s;
	_mcp = mcp;
	_M0 = PIN_M0;
	_M1 = PIN_M1;
	_AUX = PIN_AUX;
	_rt = ReadTimeout;
}



//Initialize the unit
bool EBYTE::init() {
 uint8_t okRm, okParam;
    CompleteTask(); // wait until aux pin goes back low
    _mcp -> pinMode(_AUX, INPUT);
	_mcp -> pinMode(_M0, OUTPUT);
	_mcp -> pinMode(_M1, OUTPUT);
	SetMode(MODE_NORMAL);
	_s->setTimeout(_rt);
	okRm = ReadModelData();
	okParam = ReadParameters();
	if (/*!okRm ||*/ !okParam) return false; //  get the module data, get parameters
	else return true;
}



//Method to indicate availability
bool EBYTE::available() {
	return _s->available();
}



//Method to clear Serial buffer
void EBYTE::Clear() {
    while (available()) _s->read();
}




//Method to send a chunk of data provided data is in a struct
bool EBYTE::SendStruct(const void *TheStructure, uint16_t size_) {
		CompleteTask();
		_buf = _s->write((uint8_t *) TheStructure, size_);
		CompleteTask();
		return (_buf == size_);
}



//Method to get a chunk of data provided data is in a struct
bool EBYTE::GetStruct(const void *TheStructure, uint16_t size_) {	
    CompleteTask();
	_buf = _s->readBytes((uint8_t *) TheStructure, size_);
	CompleteTask();
	return (_buf == size_);
}



//Utility method to wait until module is done tranmitting with a timeout
void EBYTE::CompleteTask() {
	while (!_mcp -> digitalRead(_AUX))
	  SmartDelay(1);
	SmartDelay(25); // per data sheet control after aux goes high is 2ms so delay for at least that long)
}



//method to set the mode (program, normal, etc.)
void EBYTE::SetMode(uint8_t mode) {
    CompleteTask(); // wait until aux pin goes back low
	SmartDelay(40); // data sheet claims module needs some extra time after mode setting (2ms)
	switch (mode){
	  case MODE_NORMAL:
	     _mcp -> digitalWrite(_M0, LOW);
		 _mcp -> digitalWrite(_M1, LOW);
		 break;
	  case MODE_WAKEUP:
	 	 _mcp -> digitalWrite(_M0, HIGH);
	 	 _mcp -> digitalWrite(_M1, LOW);
		 break;
	  case MODE_POWERDOWN:
		 _mcp -> digitalWrite(_M0, LOW);
		 _mcp -> digitalWrite(_M1, HIGH);
		 break;
	  case MODE_PROGRAM:
		 _mcp -> digitalWrite(_M0, HIGH);
		 _mcp -> digitalWrite(_M1, HIGH);
		 break;
	}
    
	SmartDelay(40); // data sheet says 2ms later control is returned, let's give just a bit more time
	CompleteTask(); // wait until aux pin goes back low
}



//delay() in a library is not a good idea as it can stop interrupts just poll internal time until timeout is reached
void EBYTE::SmartDelay(uint32_t timeout) {
	uint32_t t = millis();
	while ((millis() - t) < timeout) delay(1);	
}



//method to set the high bit of the address
void EBYTE::SetAddressH(uint8_t val) {
	_AddressHigh = val;
}



//method to set the lo bit of the address
void EBYTE::SetAddressL(uint8_t val) {
	_AddressLow = val;
}



//method to set the channel
void EBYTE::SetChannel(uint8_t val) {
	_Channel = val;
}



//method to set the air data rate
void EBYTE::SetAirDataRate(uint8_t val) {
	_AirDataRate = val;
	BuildSpeedByte();
}



//method to set the FEC
void EBYTE::SetFEC(uint8_t val) {
	if(val) _OptionFEC = 1;
	else _OptionFEC = 0;
	BuildOptionByte();
}



//method to set the Pullup
void EBYTE::SetPullup(uint8_t val) {
	if(val) _OptionPullup = 1;
	else _OptionPullup = 0;
	BuildOptionByte();
}



//method to set the Fixed=1/Transparent=0 mode
void EBYTE::SetFixedMode(uint8_t val) {
	if(val) _OptionTrans = 1;
	else _OptionTrans = 0;
	BuildOptionByte();
}



//method to set the transmit power
void EBYTE::SetTransmitPower(uint8_t val) {
	_OptionPower = val;
	BuildOptionByte();
}



//method to set the parity bit
void EBYTE::SetParityBit(uint8_t val) {
	_ParityBit = val;
	BuildSpeedByte();
}



//method to set the wake up on timing bit
void EBYTE::SetWORTIming(uint8_t val) {
	_OptionWakeup = val;
	BuildOptionByte();
}



//method to compute the address based on high and low bits
void EBYTE::SetAddress(uint16_t Val) {
	_AddressHigh = ((Val & 0xFFFF) >> 8);
	_AddressLow = (Val & 0xFF);
}



//set the UART baud rate
void EBYTE::SetUARTBaudRate(uint8_t val) {
	_UARTDataRate = val;
	BuildSpeedByte();
}



//method to build the byte for programming (notice it's a collection of a few variables)
void EBYTE::BuildSpeedByte() {
	_Speed = ((_ParityBit & 0xFF) << 6) | ((_UARTDataRate & 0xFF) << 3) | (_AirDataRate);
}



//method to build the option byte for programming (notice it's a collection of a few variables)
void EBYTE::BuildOptionByte() {
	_Options = ((_OptionTrans & 0xFF) << 7) | ((_OptionPullup & 0xFF) << 6) | ((_OptionWakeup & 0xFF) << 3) | ((_OptionFEC & 0xFF) << 2) | (_OptionPower);
}


//method to save parameters to the module
void EBYTE::SaveParameters(uint8_t val) {
	SetMode(MODE_PROGRAM);
	_Save = val;
	_s->write(_Save);
	_s->write(_AddressHigh);
	_s->write(_AddressLow);
	_s->write(_Speed);
	_s->write(_Channel);
	_s->write(_Options);
	CompleteTask();
	SetMode(MODE_NORMAL);
}



//method to read parameters, 
bool EBYTE::ReadParameters() {
  uint16_t hash1, hash2;
  
    for (uint8_t i=0; i<sizeof(_Params); i++) _Params[i] = random (255);	
	SetMode(MODE_PROGRAM);
	
    hash1 = 0;
	for (uint8_t i=0; i<sizeof(_Params); i++) hash1 += _Params[i];	
	
	_s->write(0xC1);
	_s->write(0xC1);
	_s->write(0xC1);
    CompleteTask();
    
	_s->readBytes((uint8_t*)&_Params, (uint16_t) sizeof(_Params));
	CompleteTask();
	SetMode(MODE_NORMAL);
     
    hash2 = 0;
	for (uint8_t i=0; i<sizeof(_Params); i++) hash2 += _Params[i];
	
	if (hash1 == hash2) return false;
		
	_Save = _Params[0];
	_AddressHigh = _Params[1];
	_AddressLow = _Params[2];
	_Speed = _Params[3];
	_Channel = _Params[4];
	_Options = _Params[5];

	_Address =  (_AddressHigh << 8) | (_AddressLow);
	_ParityBit = (_Speed & 0XC0) >> 6;
	_UARTDataRate = (_Speed & 0X38) >> 3;
	_AirDataRate = _Speed & 0X07;

	_OptionTrans = (_Options & 0X80) >> 7;
	_OptionPullup = (_Options & 0X40) >> 6;
	_OptionWakeup = (_Options & 0X38) >> 3;
	_OptionFEC = (_Options & 0X07) >> 2;
	_OptionPower = (_Options & 0X03);

	return true;
}



bool EBYTE::ReadModelData() {
  uint16_t hash1, hash2;
    for (uint8_t i=0; i<sizeof(_Params); i++) _Params[i] = random (255);
    SetMode(MODE_PROGRAM);

    hash1 = 0;
	for (uint8_t i=0; i<sizeof(_Params); i++) hash1 += _Params[i];	
	
	_s->write(0xC3);
	_s->write(0xC3);
	_s->write(0xC3);
    CompleteTask();
    
	_s->readBytes((uint8_t*)& _Params, (uint16_t) sizeof(_Params));
	CompleteTask(); // wait until aux pin goes back low
    SetMode(MODE_NORMAL);
    
    hash2 = 0;
	for (uint8_t i=0; i<sizeof(_Params); i++) hash2 += _Params[i];

	if (hash1 == hash2) return false;

	_Model = _Params[1];
	_Version = _Params[2];
	_Features = _Params[3];
	
    return true;
}


