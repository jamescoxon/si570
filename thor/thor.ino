#include <Arduino.h>
#include <util/crc16.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "thor.h"
#include "thor_varicode.h"
#include "i2c.h"

#define F_CPU (8000000)

char superbuffer [80]; //Telem string buffer
int txPin = 6; // radio tx line
int ledPin =  13; // LED 
int count = 0;
float fudgeValue = 7;

byte regAddr; // First register address (7 or 13)
byte i2cAddress = 0x55;

volatile int go_thor = 0;

#define SI570_R137_FREEZE_DCO	(1 << 4)


//#define SAMPLERATE (F_CPU / 256 / 5)
#define SAMPLERATE (12500)
#define TONES (18)

/* NASA coefficients for viterbi encode/decode algorithms */
#define THOR_K 7
#define THOR_POLYA 0x6D
#define THOR_POLYB 0x4F

/* NASA Galileo coefficients for viterbi encode/decode algorithms */
#define GALILEO_K 15
#define GALILEO_POLYA 0x4CD1
#define GALILEO_POLYB 0x52B9

/* Maximum size of the interleaver table */
#define SIZE (4)
#define INTER_MAX (50 * SIZE * SIZE / 8)

/* THOR timing */
volatile static uint16_t _sr;
volatile static uint16_t _sl;

/* Double spaced */
volatile static uint8_t _ds;

/* Convolutional encoder settings */
volatile static uint16_t _conv_polya;
volatile static uint16_t _conv_polyb;

/* Interleaver settings */
volatile static uint8_t _inter_depth;

/* THOR state */
static uint16_t _conv_sh;
static uint8_t _preamble;
static uint8_t _inter_table[INTER_MAX];

/* Message being sent */
volatile static uint8_t  _txpgm;
volatile static uint8_t *_txbuf;
volatile static uint16_t _txlen;

static inline uint8_t _rtab(uint8_t i, uint8_t j, uint8_t k)
{
	uint16_t o = (SIZE * SIZE * i) + (SIZE * j) + k;
	return((_inter_table[o >> 3] >> (o & 7)) & 1);
}

static inline void _wtab(uint8_t i, uint8_t j, uint8_t k, uint8_t v)
{
	uint16_t o = (SIZE * SIZE * i) + (SIZE * j) + k;
	if(!v) _inter_table[o >> 3] &= ~(1 << (o & 7));
	else   _inter_table[o >> 3] |= 1 << (o & 7);
}

static void _thor_interleave_symbols(uint8_t *psyms)
{
	uint8_t i, j, k;
	
	for(k = 0; k < _inter_depth; k++)
		for(i = 0; i < SIZE; i++)
		{
			for(j = 0; j < SIZE - 1; j++)
				_wtab(k, i, j, _rtab(k, i, j + 1));
			
			_wtab(k, i, SIZE - 1, psyms[i]);
			
			psyms[i] = _rtab(k, i, SIZE - i - 1);
		}
}

static void _thor_interleave(uint8_t *pbits)
{
	uint8_t i, syms[SIZE];
	
	for(i = 0; i < SIZE; i++)
		syms[i] = (*pbits >> (SIZE - i - 1)) & 1;
	
	_thor_interleave_symbols(syms);
	
	for(*pbits = i = 0; i < SIZE; i++)
		*pbits = (*pbits << 1) | syms[i];
}

/* Calculate the parity of byte x */
static inline uint8_t parity(uint8_t x)
{
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return(x & 1);
}

/* Convolutional encoder */
static uint8_t _thor_conv_encode(uint8_t bit)
{
	uint8_t r;
	
	_conv_sh = (_conv_sh << 1) | bit;
	
	r  = parity(_conv_sh & _conv_polya);
	r |= parity(_conv_sh & _conv_polyb) << 1;
	
	return(r);
}

static uint16_t _thor_lookup_code(uint8_t c, uint8_t sec)
{
	/* Primary character set */
	if(!sec) return(pgm_read_word(&_varicode[c]));
	
	/* Secondary character set (restricted range) */
	if(c >= ' ' && c <= 'z')
		return(pgm_read_word(&_varicode_sec[c - ' ']));
	
	/* Else return NUL */
	return(pgm_read_word(&_varicode[0]));
}
ISR(TIMER2_COMPA_vect)
{
	static uint16_t da = 0, db = 0;
	static uint16_t code;
	static uint8_t tone = 0;
	static uint8_t len = 0;
	uint8_t i, bit_sh;
	
	/* Timing */
	da += SAMPLERATE;
	while(da >= _sr)
	{
		db++;
		da -= SAMPLERATE;
	}
	
	if(db < _sl) return;
	db = 0;
	
	/* Transmit the tone */
        //Serial.print(tone);
        quickTone(tone);
        //Serial.print(tone);
	
	if(_preamble)
	{
		tone = (tone + 2) % TONES;
		_preamble--;
		return;
	}
	
	/* Calculate the next tone */
	bit_sh = 0;
	for(i = 0; i < 2; i++)
	{
		uint8_t data;
		
		/* Done sending the current varicode? */
		if(!len)
		{
			if(_txlen)
			{
				/* Read the next character */
				if(_txpgm == 0) data = *(_txbuf++);
				else data = pgm_read_byte(_txbuf++);
				_txlen--;
			}
			else data = 0;
			
			/* Get the varicode for this character */
			code = _thor_lookup_code(data, 0);
			len  = code >> 12;
		}
		
		/* Feed the next bit into the convolutional encoder */
		data = _thor_conv_encode((code >> --len) & 1);
		
		bit_sh = (bit_sh << 1) | (data & 1);
		bit_sh = (bit_sh << 1) | ((data >> 1) & 1);
	}
	
	_thor_interleave(&bit_sh);
	tone = (tone + 2 + bit_sh) % TONES;
}

void thor_init(thor_mode_t mode)
{
	/* Clear the THOR state */
	_conv_sh = 0;
	_txpgm = 0;
	_txbuf = NULL;
	_txlen = 0;
	_preamble = 16;
	
	/* Setup per-mode parameters */
	switch(mode)
	{
	case THOR4:
		_sr = 8000;
		_sl = 2048;
		_ds = 2;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR5:
		_sr = 11025;
		_sl = 2048;
		_ds = 2;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR8:
		_sr = 8000;
		_sl = 1024;
		_ds = 2;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR11:
		_sr = 11025;
		_sl = 1024;
		_ds = 1;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR16:
		_sr = 8000;
		_sl = 512;
		_ds = 1;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR22:
		_sr = 11025;
		_sl = 512;
		_ds = 1;
		_conv_polya = THOR_POLYA;
		_conv_polyb = THOR_POLYB;
		_inter_depth = 10;
		break;
	
	case THOR25x4:
		_sr = 8000;
		_sl = 320;
		_ds = 4;
		_conv_polya = GALILEO_POLYA;
		_conv_polyb = GALILEO_POLYB;
		_inter_depth = 50;
		break;
	
	case THOR50x1:
		_sr = 8000;
		_sl = 160;
		_ds = 1;
		_conv_polya = GALILEO_POLYA;
		_conv_polyb = GALILEO_POLYB;
		_inter_depth = 50;
		break;
	
	case THOR50x2:
		_sr = 8000;
		_sl = 160;
		_ds = 2;
		_conv_polya = GALILEO_POLYA;
		_conv_polyb = GALILEO_POLYB;
		_inter_depth = 50;
		break;
	
	case THOR100:
		_sr = 8000;
		_sl = 80;
		_ds = 1;
		_conv_polya = GALILEO_POLYA;
		_conv_polyb = GALILEO_POLYB;
		_inter_depth = 50;
		break;
	
	default:
		return;
	}
	
	memset(_inter_table, 0, INTER_MAX);
        
	/* Fast PWM mode, non-inverting output on OC2A */
	//TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
	//TCCR2B = _BV(CS20);
	//OCR2A = 0x80;
        
        cli();
        
        TCCR2A = 0;// set entire TCCR2A register to 0
        TCCR2B = 0;// same for TCCR2B
 /* 
        //OCR2A = F_CPU / 64 / SAMPLERATE - 1;
        OCR2A = 4;
	TCCR2A |= _BV(WGM21); // Mode 2, CTC
        TCCR2B |= _BV(CS21) | _BV(CS22); //pre-scaler = 256
	TIMSK2 |= _BV(OCIE2A);
*/

        TCCR2A = _BV(WGM21); /* Mode 2, CTC */
        TCCR2B = _BV(CS22) | _BV(CS20);/* prescaler 128 */
        OCR2A = F_CPU / 128 / SAMPLERATE - 1;
        TIMSK2 = _BV(OCIE2A); /* Enable interrupt */
        
         sei();

}

void inline thor_wait(void)
{
	/* Wait for interrupt driven TX to finish */
	while(_txlen > 0) while(_txlen > 0);
}

void thor_data(uint8_t *data, size_t length)
{
	thor_wait();
	_txpgm = 0;
	_txbuf = data;
	_txlen = length;
}

void thor_data_P(PGM_P data, size_t length)
{
	thor_wait();
	_txpgm = 1;
	_txbuf = (uint8_t *) data;
	_txlen = length;
}

void thor_string(char *s)
{
	uint16_t length = strlen(s);
	thor_data((uint8_t *) s, length);
}

void thor_string_P(PGM_P s)
{
	uint16_t length = strlen_P(s);
	thor_data_P(s, length);
}

byte freezeDCO(void) {
	int idco;
	 // Freeze DCO
        idco = i2cReadRegister(0x55, 137);
	i2cWriteRegister (0x55, 137, idco | 0x10 );
}

byte unfreezeDCO(void) {
        int idco;
	// Unfreeze DCO
        idco = i2cReadRegister(0x55, 137);
	i2cWriteRegister (0x55, 137, idco & 0xEF );
}

void quickTone(uint8_t aTone){
  
  byte i2cWriteBuf[] = {0xE1, 0xC2, 0xB5, 0xA9, 0x01, 0xC4};
  int toneValue = (0x01 + int(aTone * fudgeValue));
  //Serial.println(toneValue);
  i2cWriteRegister(0x55, 0xb, toneValue);
}

void quickFreq(byte x){

  byte i2cWriteBuf[] = {0xE1, 0xC2, 0xB5, 0xA9, 0xD9, 0xC4};
 
    if(x == 1){
    //i2cWriteBuf[0] = 0xE8;
    //i2cWriteBuf[1] = 0x42;
    //i2cWriteBuf[2] = 0xC6;
    i2cWriteBuf[3] = 0xA4;
    //i2cWriteBuf[4] = 0x8B;
    //i2cWriteBuf[5] = 0xBA;
  }
  
  i2cWriteRegisters(0x55, 0x07, 6, i2cWriteBuf);
}

void setFrequency(byte x){
  
  //Freeze DCO
  freezeDCO();
  
  //Send the stuff here
  
  //E1 C2 B5 A9 D9 C4
  byte i2cWriteBuf[] = {0xE1, 0xC2, 0xB5, 0xA9, 0xD9, 0xC4};

  //byte i2cWriteBuf[] = {0xA2, 0x42, 0xAB, 0x3A, 0x62, 0xE7};
  //28.200Mhz E3 C2 B4 2F 7D 1E
  //byte i2cWriteBuf[] = {0xE3, 0xC2, 0xB4, 0x2F, 0x7D, 0x1E};
  
  //10.140Mhz EA C2 AF 29 0E EC
  //10.137Mhz EA C2 AE E3 B1 23
  //byte i2cWriteBuf[] = {0xEA, 0xC2, 0xAE, 0xE3, 0xB1, 0x23};
  
  //13.556Mhz E8 42 C5 CC 4D 26
  //byte i2cWriteBuf[] = {0xE8, 0x42, 0xC5, 0xCC, 0x4D, 0x26};
  
  //13.553
  //E8 42 C4 C0 4B 86
  //byte i2cWriteBuf[] = {0xE8, 0x42, 0xC5, 0xA0, 0x4D, 0x26};

  //434.00Mhz 40 42 D8 E9 35 68
  //byte i2cWriteBuf[] = {0x40, 0x42, 0xD8, 0xE9, 0x35, 0x68};
  
  //144.100Mhz A0 C2 D6 0E 48 40
  //byte i2cWriteBuf[] = {0xA0, 0xC2, 0xD6, 0x0E, 0x48, 0x40};    
  
  i2cWriteRegisters(0x55, 0x07, 6, i2cWriteBuf);
  
  //Unfreeze DCO
  unfreezeDCO();
}

uint16_t gps_CRC16_checksum (char *string)
{
  size_t i;
  uint16_t crc;
  uint8_t c;

  crc = 0xFFFF;

  // Calculate checksum ignoring the first two $s
  for (i = 2; i < strlen(string); i++)
  {
    c = string[i];
    crc = _crc_xmodem_update (crc, c);
  }

  return crc;
}

void prepData() {
  //if(gpsstatus == 1){
    //gps_check_lock();
    //gps_get_position();
    //gps_get_time();
  //}
  count++;
  int n;
  n=sprintf (superbuffer, "$$ATLAS,%d,51.4980,-0.0532,20,SLOWHELL-RTTY", count);
  n = sprintf (superbuffer, "%s*%04X\n", superbuffer, gps_CRC16_checksum(superbuffer));
}

void setup() {
        byte err;
	Serial.begin(115200);
        pinMode(txPin, OUTPUT);
        digitalWrite(txPin, HIGH);
        
        //Setup I2C
        i2cInit();
        i2cSetBitrate(400);  // try 100kHz
        
	Serial.println("OK");
        
        //check we are working
        uint8_t data = i2cReadRegister(0x55, 137);
        Serial.print("0x"); Serial.println(data);
        digitalWrite(txPin, LOW);
        
        setFrequency(0);
        
        digitalWrite(txPin, HIGH);
        delay(2500);
        quickFreq(1);
        delay(2500);
        digitalWrite(txPin, LOW);
        quickFreq(0);
        
        thor_init(THOR11);
	
	thor_data_P(PSTR("\0\x0D\x02\x0D"), 4);

        Serial.println("Setup Complete");
        
        digitalWrite(txPin, HIGH);
}

void loop() {
  while(1){
    thor_string_P(PSTR("Hello, World!TTTTTTTTTTTT\n"));
    delay(10000);
    fudgeValue = fudgeValue + 0.1;
    Serial.println(fudgeValue);
  }
    
}

