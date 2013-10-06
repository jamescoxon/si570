#include <Wire.h>
#include <Arduino.h>

char superbuffer [80]; //Telem string buffer
int txPin = 6; // radio tx line
int ledPin =  13; // LED 

byte regAddr; // First register address (7 or 13)
byte i2cAddress = 0x55;

#define SI570_R137_FREEZE_DCO	(1 << 4)

struct t_htab { char c; int hellpat[5]; } ;

struct t_htab helltab[] = {

  {'1', { B00000100, B00000100, B01111100, B00000000, B00000000 } },
  {'2', { B01001000, B01100100, B01010100, B01001100, B01000000 } },
  {'3', { B01000100, B01000100, B01010100, B01010100, B00111100 } },
  {'4', { B00011100, B00010000, B00010000, B01111100, B00010000 } },
  {'5', { B01000000, B01011100, B01010100, B01010100, B00110100 } },
  {'6', { B00111100, B01010010, B01001010, B01001000, B00110000 } },
  {'7', { B01000100, B00100100, B00010100, B00001100, B00000100 } },
  {'8', { B00111100, B01001010, B01001010, B01001010, B00111100 } },
  {'9', { B00001100, B01001010, B01001010, B00101010, B00111100 } },
  {'0', { B00111000, B01100100, B01010100, B01001100, B00111000 } },
  {'A', { B01111000, B00101100, B00100100, B00101100, B01111000 } },
  {'B', { B01000100, B01111100, B01010100, B01010100, B00101000 } },
  {'C', { B00111000, B01101100, B01000100, B01000100, B00101000 } },
  {'D', { B01000100, B01111100, B01000100, B01000100, B00111000 } },
  {'E', { B01111100, B01010100, B01010100, B01000100, B01000100 } },
  {'F', { B01111100, B00010100, B00010100, B00000100, B00000100 } },
  {'G', { B00111000, B01101100, B01000100, B01010100, B00110100 } },
  {'H', { B01111100, B00010000, B00010000, B00010000, B01111100 } },
  {'I', { B00000000, B01000100, B01111100, B01000100, B00000000 } },
  {'J', { B01100000, B01000000, B01000000, B01000000, B01111100 } },
  {'K', { B01111100, B00010000, B00111000, B00101000, B01000100 } },
  {'L', { B01111100, B01000000, B01000000, B01000000, B01000000 } },
  {'M', { B01111100, B00001000, B00010000, B00001000, B01111100 } },
  {'N', { B01111100, B00000100, B00001000, B00010000, B01111100 } },
  {'O', { B00111000, B01000100, B01000100, B01000100, B00111000 } },
  {'P', { B01000100, B01111100, B01010100, B00010100, B00011000 } },
  {'Q', { B00111000, B01000100, B01100100, B11000100, B10111000 } },
  {'R', { B01111100, B00010100, B00010100, B00110100, B01011000 } },
  {'S', { B01011000, B01010100, B01010100, B01010100, B00100100 } },
  {'T', { B00000100, B00000100, B01111100, B00000100, B00000100 } },
  {'U', { B01111100, B01000000, B01000000, B01000000, B01111100 } },
  {'V', { B01111100, B00100000, B00010000, B00001000, B00000100 } },
  {'W', { B01111100, B01100000, B01111100, B01000000, B01111100 } },
  {'X', { B01000100, B00101000, B00010000, B00101000, B01000100 } },
  {'Y', { B00000100, B00001000, B01110000, B00001000, B00000100 } },
  {'Z', { B01000100, B01100100, B01010100, B01001100, B01100100 } },
  {'.', { B01000000, B01000000, B00000000, B00000000, B00000000 } },
  {'-', { B00010000, B00010000, B00010000, B00010000, B00010000 } },
  {',', { B10000000, B10100000, B01100000, B00000000, B00000000 } },
  {'/', { B01000000, B00100000, B00010000, B00001000, B00000100 } },
  {'@', { B00111000, B01000100, B01010100, B01000100, B00111000 } },
  {'*', { B00000000, B00000000, B00000100, B00001110, B00000100 } }

};

#define N_HELL  (sizeof(helltab)/sizeof(helltab[0]))

void helldelay()
{
  //Slow
  delay(64);
  delayMicroseconds(900);
  
  //Feld-Hell
  //delay(8);
  //delayMicroseconds(160);

}

void on()
{
  digitalWrite(txPin, HIGH);
  helldelay();
  digitalWrite(txPin, LOW);
}

void hellsend(char c)
{
  int i,j,q ;
  if (c == ' ') {
      for (int d=0; d<14; d++){
        helldelay();  
      }
    return ;
  }
  for (i=0; i<N_HELL; i++) {
    if (helltab[i].c == c) {
      //Serial.print(helltab[i].c) ;
      
      for (j=0; j<=4; j++) 
      {
        byte mask = B10000000;
        for (q=0; q<=6; q++)
        {      
          if(helltab[i].hellpat[j] & mask) {
            on();
          } else {
            helldelay();
          }
          mask >>= 1;
        }
      }
      for (int d=0; d<14; d++){
        helldelay();  
      }
      return ;
    }
  }
  /* if we drop off the end, then we send a space */
  //Serial.print("?") ;
}

void hellsendmsg(char *str)
{
  while (*str)
    hellsend(*str++) ;
  //Serial.println("");
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
	Wire.endTransmission(false);
	Wire.requestFrom((byte) i2cAddress, (byte) 1);
	resp = Wire.read();
	Wire.endTransmission();
        return resp;
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
  
  //Freeze DCO
  freezeDCO();
  
  Wire.beginTransmission(0x55);
  //Send the stuff here
  //28.200Mhz E3 C2 B4 2F 7D 1E
  //byte i2cWriteBuf[] = {0xE3, 0xC2, 0xB4, 0x2F, 0x7D, 0x1E};
  
  //10.140Mhz EA C2 AF 29 0E EC
  //10.137Mhz EA C2 AE E3 B1 23
  //byte i2cWriteBuf[] = {0xEA, 0xC2, 0xAE, 0xE3, 0xB1, 0x23};
  
  //13.560Mhz A2 42 AB 3A 62 E7
  // E8 42 C5 CC 4D 26

  byte i2cWriteBuf[] = {0xE8, 0x42, 0xC5, 0xCC, 0x4D, 0x26};

  
  //434.00Mhz 40 42 D8 E9 35 68
  //byte i2cWriteBuf[] = {0x40, 0x42, 0xD8, 0xE9, 0x35, 0x68};
  
  //144.100Mhz A0 C2 D6 0E 48 40
  //byte i2cWriteBuf[] = {0xA0, 0xC2, 0xD6, 0x0E, 0x48, 0x40};    
  
  Wire.write(0x07);
  	for (byte i = 0; i < 6 ; ++i) {
  		Wire.write(i2cWriteBuf[i]);
  	}
  Wire.endTransmission();
  
  //Unfreeze DCO
  unfreezeDCO();
}

void setup() {
        byte err;
	Serial.begin(9600);
        pinMode(txPin, OUTPUT);
        digitalWrite(txPin, HIGH);
        Wire.begin();
	Serial.println("OK");

        setFrequency(0);
        
        //check we are working
        Serial.println(readRegister(137));
        digitalWrite(txPin, LOW);
}

void loop() {
    hellsendmsg("ATLAS TEST LONDON");
    //Make sure transmitter is off
    digitalWrite(txPin, LOW);
    //Sleep
    delay(5000);     
}

