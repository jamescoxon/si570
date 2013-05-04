#include <Wire.h>
#include <Arduino.h>


char superbuffer [80]; //Telem string buffer
int txPin = 6; // radio tx line
int ledPin =  13; // LED 
int mode = 0;

byte regAddr;					// First register address (7 or 13)
byte i2cAddress = 0x55;

#define SI570_R137_FREEZE_DCO	(1 << 4)

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
  //10.140016Mhz
  //byte i2cWriteBuf[] = {0xEA, 0xC2, 0xAE, 0xFC, 0x05, 0xAE};
  
   byte i2cWriteBuf[] = {0xEA, 0xC2, 0xAE, 0xF3, 0xD4, 0x87};
  //Freeze DCO
  freezeDCO();
  
  Wire.beginTransmission(i2cAddress);
  //Send the stuff here
  //28.200Mhz E3 C2 B4 2F 7D 1E
  //byte i2cWriteBuf[] = {0xE3, 0xC2, 0xB4, 0x2F, 0x7D, 0x1E};
  
  //10.140Mhz EA C2 AF 29 0E EC
  //10.137Mhz EAC2AEE3B123
  
  //EAC2AEF3D487

  if(x == 1){
    i2cWriteBuf[3] = 0xAE;
    i2cWriteBuf[3] = 0xF3;
    i2cWriteBuf[4] = 0xD4;
    i2cWriteBuf[5] = 0x87;
  }
  
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
        

}

void loop() {
    sendmsg("M6JCX JO01AL");
    delay(30000);
    

}

