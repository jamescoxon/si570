/*
  rtty1.ino - code to interface arduino and a si570 and generate a ~10mW RTTY signal
  James Coxon - jacoxon@googlemail.com
  4/5/13
*/
//Elements of the code taken from 
//https://github.com/gaftech/ArduinoSi570
/*
 * Si570.cpp and Si570.h
 *
 * Copyright (c) 2012, Gabriel Fournier <gabriel@gaftech.fr>
 *
 * This file is part of ArduinoSi570 project.
 * Please read attached LICENSE file. 
 *
 *  Created on: 28 août 2012
 *      Author: Gabriel Fournier
 */
 
//https://github.com/steamfire/WSWireLib/tree/master/Library/WSWire
#include <WSWire.h>
#include <Arduino.h>


char superbuffer [80]; //Telem string buffer
int txPin = 6; // radio tx line
int ledPin =  13; // LED 
int mode = 0;
int count = 0;
int delay_time = 50;
byte regAddr;					// First register address (7 or 13)
byte i2cAddress = 0x55;

#define SI570_R137_FREEZE_DCO	(1 << 4)
#define SI570_R135_FREEZE_M		(1 << 4)
#define SI570_R135_DEFAULT		0

struct t_mtab { char c, pat; } ;

struct t_mtab morsetab[] = {
  	{'.', 106},
	{',', 115},
	{'?', 76},
	{'/', 41},
        {'-', 97},
	{'A', 6},
	{'B', 17},
	{'C', 21},
	{'D', 9},
	{'E', 2},
	{'F', 20},
	{'G', 11},
	{'H', 16},
	{'I', 4},
	{'J', 30},
	{'K', 13},
	{'L', 18},
	{'M', 7},
	{'N', 5},
	{'O', 15},
	{'P', 22},
	{'Q', 27},
	{'R', 10},
	{'S', 8},
	{'T', 3},
	{'U', 12},
	{'V', 24},
	{'W', 14},
	{'X', 25},
	{'Y', 29},
	{'Z', 19},
	{'1', 62},
	{'2', 60},
	{'3', 56},
	{'4', 48},
	{'5', 32},
	{'6', 33},
	{'7', 35},
	{'8', 39},
	{'9', 47},
	{'0', 63}
} ;
#define N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))

#define SPEED  (1)
#define DOTLEN  (10000)
#define DASHLEN  (30000)

//#define DOTLEN  (1200/SPEED)
//#define DASHLEN  (3*(1200/SPEED))

void
dash()
{
  digitalWrite(txPin, HIGH);
  digitalWrite(ledPin, HIGH);
  delay(DASHLEN);
  digitalWrite(txPin, LOW);
  digitalWrite(ledPin, LOW);
  delay(DOTLEN) ;
}

void
dit()
{
  digitalWrite(txPin, HIGH);
  digitalWrite(ledPin, HIGH);
  delay(DOTLEN);
  digitalWrite(txPin, LOW);
  digitalWrite(ledPin, LOW);
  delay(DOTLEN);
}

void
send(char c)
{
  int i ;
  if (c == ' ') {
    Serial.print(c) ;
    delay(7*DOTLEN) ;
    return ;
  }
  for (i=0; i<N_MORSE; i++) {
    if (morsetab[i].c == c) {
      unsigned char p = morsetab[i].pat ;
      Serial.print(morsetab[i].c) ;

      while (p != 1) {
          if (p & 1)
            dash() ;
          else
            dit() ;
          p = p / 2 ;
      }
      delay(2*DOTLEN) ;
      return ;
    }
  }
  /* if we drop off the end, then we send a space */
  Serial.print("?") ;
}

void
sendmsg(char *str)
{
  while (*str)
    send(*str++) ;
  Serial.println("");
}


// RTTY Functions - from RJHARRISON's AVR Code
void rtty_txstring (char * string)
{

	/* Simple function to sent a char at a time to 
	** rtty_txbyte function. 
	** NB Each char is one byte (8 Bits)
	*/
	char c;
	c = *string++;
	while ( c != '\0')
	{
		rtty_txbyte (c);
		c = *string++;
	}
}

void rtty_txbyte (char c)
{
	/* Simple function to sent each bit of a char to 
	** rtty_txbit function. 
	** NB The bits are sent Least Significant Bit first
	**
	** All chars should be preceded with a 0 and 
	** proceded with a 1. 0 = Start bit; 1 = Stop bit
	**
	** ASCII_BIT = 7 or 8 for ASCII-7 / ASCII-8
	*/
	int i;
	rtty_txbit (0); // Start bit
	// Send bits for for char LSB first	
	for (i=0;i<8;i++)
	{
		if (c & 1) rtty_txbit(1); 
			else rtty_txbit(0);	
		c = c >> 1;
	}
	rtty_txbit (1); // Stop bit
        rtty_txbit (1); // Stop bit
}

void rtty_txbit (int bit)
{
		if (bit)
		{
		  // high
                  digitalWrite(txPin, LOW);
                  setFrequency(1);
                  digitalWrite(txPin, HIGH);
		}
		else
		{
		  // low
                  digitalWrite(txPin, LOW);
                  setFrequency(0);
                  digitalWrite(txPin, HIGH);
		}
                delayMicroseconds(10000); // 10000 = 100 BAUD 20150 19500
                delayMicroseconds(8000);

}

byte writeRegister(byte byteAddress, byte value) {

	// IMPORTANT: This method must run fast
	// because used in the "Unfreeze DCO + Assert new freq" sequence
	// So don't debug here !

	Wire.beginTransmission(i2cAddress);
	Wire.write(byteAddress);
	Wire.write(value);
	Wire.endTransmission();

}

byte readRegister(byte byteAddress) {
	byte resp;

	Wire.beginTransmission(i2cAddress);
	Wire.write(byteAddress);
	//NOTE: Needs arduino libs >= 1.0.1 (use of sendStop option)
	Wire.endTransmission();
	Wire.requestFrom((byte) i2cAddress, (byte) 1);
	resp = Wire.read();
	Wire.endTransmission();
}

byte freezeDCO(void) {
	int idco;
	 // Freeze DCO
        idco = readRegister( 137 );
	writeRegister (137, idco | 0x10 );
}

byte unfreezeDCO(void) {
        int idco;
	// Unfreeze DCO
        idco = readRegister( 137 );
	writeRegister (137, idco & 0xEF );
}

void setFrequency(byte x){
  byte err;
  
  //13.5576Mhz = A9 C2 AB 4E AB EA
  byte i2cWriteBuf[] = {0xA9, 0xC2, 0xAB, 0x4E, 0xAB, 0xEA};
  
    //13.558Mhz = A9 C2 AB 53 D5 20
    if(x == 1){
    i2cWriteBuf[0] = 0xA9;
    i2cWriteBuf[1] = 0xC2;
    i2cWriteBuf[2] = 0xAB;
    i2cWriteBuf[3] = 0x53;
    i2cWriteBuf[4] = 0xD5;
    i2cWriteBuf[5] = 0x20;
  }
  
   //byte i2cWriteBuf[] = {0xEA, 0xC2, 0xAE, 0xF3, 0xD4, 0x87};
  //Freeze DCO
  //freezeDCO();
  
  err |= writeRegister(135, SI570_R135_FREEZE_M);
  
  Wire.beginTransmission(i2cAddress);
   
  Wire.write(0x07);
  	for (byte i = 0; i < 6 ; ++i) {
  		Wire.write(i2cWriteBuf[i]);
  	}
  Wire.endTransmission();
  
  err |= writeRegister(135, SI570_R135_DEFAULT);
  //Unfreeze DCO
  //unfreezeDCO();
}

void setup() {
	Serial.begin(9600);
        pinMode(txPin, OUTPUT);
        Wire.begin();
	Serial.println("OK");

        setFrequency(0);
        digitalWrite(txPin, HIGH);
        delay(3000);
        digitalWrite(txPin, LOW);
        delay(3000);
        
        setFrequency(1);
        digitalWrite(txPin, HIGH);
        delay(3000);
        digitalWrite(txPin, LOW);
        delay(3000);
        
        setFrequency(0);
        
digitalWrite(txPin, HIGH);
}

void loop() {
    //Enable the si570 early
    delay(1000);
    rtty_txstring("$$$$M6JCX,HIGH,ALTITUDE,BALLOON\n");
    digitalWrite(txPin, LOW);
    
    delay(3000);
    digitalWrite(txPin, HIGH);
}

  //Some additional frequencies
  //28.200Mhz E3 C2 B4 2F 7D 1E
  //byte i2cWriteBuf[] = {0xE3, 0xC2, 0xB4, 0x2F, 0x7D, 0x1E};
  
  //10.140Mhz EA C2 AF 29 0E EC
  //10.137Mhz EAC2AEE3B123
  
  //EAC2AEF3D487
  
  //434.00Mhz 40 42 D8 E9 35 68
  //byte i2cWriteBuf[] = {0x40, 0x42, 0xD8, 0xE9, 0x35, 0x68};
  
  //144.100Mhz A0 C2 D6 0E 48 40
  //byte i2cWriteBuf[] = {0xA0, 0xC2, 0xD6, 0x0E, 0x48, 0x40};  
