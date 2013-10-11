//http://users.soe.ucsc.edu/~karplus/Arduino/libraries/i2c/

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 

// TWSR values (not bits)
// (taken from avr-libc twi.h - thank you Marek Michalkiewicz)
// Master
#define TW_START					0x08
#define TW_REP_START				0x10
// Master Transmitter
#define TW_MT_SLA_ACK				0x18
#define TW_MT_SLA_NACK				0x20
#define TW_MT_DATA_ACK				0x28
#define TW_MT_DATA_NACK				0x30
#define TW_MT_ARB_LOST				0x38
// Master Receiver
#define TW_MR_ARB_LOST				0x38
#define TW_MR_SLA_ACK				0x40
#define TW_MR_SLA_NACK				0x48
#define TW_MR_DATA_ACK				0x50
#define TW_MR_DATA_NACK				0x58
// Slave Transmitter
#define TW_ST_SLA_ACK				0xA8
#define TW_ST_ARB_LOST_SLA_ACK		0xB0
#define TW_ST_DATA_ACK				0xB8
#define TW_ST_DATA_NACK				0xC0
#define TW_ST_LAST_DATA				0xC8
// Slave Receiver
#define TW_SR_SLA_ACK				0x60
#define TW_SR_ARB_LOST_SLA_ACK		0x68
#define TW_SR_GCALL_ACK				0x70
#define TW_SR_ARB_LOST_GCALL_ACK	0x78
#define TW_SR_DATA_ACK				0x80
#define TW_SR_DATA_NACK				0x88
#define TW_SR_GCALL_DATA_ACK		0x90
#define TW_SR_GCALL_DATA_NACK		0x98
#define TW_SR_STOP					0xA0
// Misc
#define TW_NO_INFO					0xF8
#define TW_BUS_ERROR				0x00

// defines and constants
#define TWCR_CMD_MASK		0x0F
#define TWSR_STATUS_MASK	0xF8

// return values
#define I2C_OK				0x00
#define I2C_ERROR_NODEV		0x01

// functions

//! Initialize I2C (TWI) interface
//	Must be done before anything else is called in this package
void i2cInit(void);

//! Set the I2C transaction bitrate (in KHz)
void i2cSetBitrate(unsigned short bitrateKHz);

// High-level interactions:
// Read one register 
uint8_t i2cReadRegister(uint8_t i2c_7bit_address, uint8_t address);
// Read num_to_read registers starting at address
void i2cReadRegisters(uint8_t i2c_7bit_address, uint8_t address, uint8_t num_to_read, uint8_t * dest);

// Write one register
void i2cWriteRegister(uint8_t i2c_7bit_address, uint8_t address, uint8_t data);
// Write num_to_write registers, starting at address
void i2cWriteRegisters(uint8_t i2c_7bit_address, uint8_t address, uint8_t num_to_write, const uint8_t *data);




// Low-level I2C transaction commands 
//! Send an I2C start condition in Master mode
void i2cSendStart(void);

//! Send an I2C stop condition in Master mode
void i2cSendStop(void);

//! Wait for current I2C operation to complete (not used for STOP)
// Return 0 if times out rather than completing.
//	Already included in operations, so not needed externally.
boolean i2cWaitForComplete();

//! Send an (address|R/W) combination or a data byte over I2C
//	Deprecated: use i2cSendWriteAddress, i2cSendReadAddress, i2cSendData instead
void i2cSendByte(uint8_t data);

void i2cSendWriteAddress(uint8_t  i2c_7bit_address);
void i2cSendReadAddress(uint8_t  i2c_7bit_address);
void i2cSendData(uint8_t data);	// used for both data and register addresses

//! Receive a data byte over I2C  
// ackFlag = 1 if recevied data should be ACK'ed
// ackFlag = 0 if recevied data should be NACK'ed
void i2cReceiveByte(uint8_t ackFlag=1);
//! Pick up the data that was received with i2cReceiveByte()
uint8_t i2cGetReceivedByte(void);
//! Get current I2c bus status from TWSR
uint8_t i2cGetStatus(void);

// high-level I2C transaction commands

//! send I2C data to a device on the bus (non-interrupt based)
uint8_t i2cMasterSendNI(uint8_t deviceAddr, uint8_t length, uint8_t* data);
//! receive I2C data from a device on the bus (non-interrupt based)
uint8_t i2cMasterReceiveNI(uint8_t deviceAddr, uint8_t length, uint8_t *data);

/*********************
 ****I2C Functions****
 *********************/

// checks for ACK after TWINT indicates operation complete
#define START_OK  ((TWSR & 0xF8) == TW_START)
#define REPEATED_START_OK ((TWSR&0xF8)==TW_REP_START)
#define WRITE_ADDRESS_OK ((TWSR&0xF8) == TW_MT_SLA_ACK)
#define READ_ADDRESS_OK ((TWSR&0xF8) == TW_MR_SLA_ACK)
#define WRITE_DATA_OK ((TWSR&0xF8) == TW_MT_DATA_ACK)

// don't check ACK bit on reading data, since we are setting it.
#define READ_DATA_OK  ((TWSR&0xF0) == TW_MR_DATA_ACK)



static void i2cError(const char*err)
{   
  /*
  Serial.print("ERROR in i2c: TWSR=0x");
	Serial.print(TWSR,HEX);
	Serial.print(" ");
*/
	//Serial.println("I2C ERROR");
	//delay(3);

	
}

void i2cInit(void)
{
	// set i2c bit rate to 100KHz
	i2cSetBitrate(100);
	// enable TWI (two-wire interface)
	sbi(TWCR, TWEN);	// Enable TWI
}

void i2cSetBitrate(unsigned short bitrateKHz)
{
	uint8_t bitrate_div;
	// set i2c bitrate
	// SCL freq = F_CPU/(prescale*(16+2*TWBR))
	
	uint8_t prescale;
	if (bitrateKHz <8)
	{   prescale=16;
		cbi(TWSR, TWPS0);
		sbi(TWSR, TWPS1);	
	}
	else if (bitrateKHz < 31)
	{   prescale = 4;
		sbi(TWSR, TWPS0);
		cbi(TWSR, TWPS1);
	}
	else 
	{   prescale=1;
		cbi(TWSR, TWPS0);
		cbi(TWSR, TWPS1);	
	}

	
	// (set prescale=4)
		
	//calculate bitrate division
	// TWBR = (F_CPU/SCL / prescale -16)/2
	//      = F_CPU/(2*prescale*SCL) - 8
	bitrate_div = F_CPU/(2000l*prescale*bitrateKHz);
	if(bitrate_div >= 8)
		bitrate_div -= 8;
	TWBR= bitrate_div;
}

// wait for TWINT to be set after every operation EXCEPT sending stop
// return 0 if times out rather than completing
boolean i2cWaitForComplete()
{
	uint16_t i = 0;		//time out variable
        uint16_t num_tries = 1000;
	
	// wait for i2c interface to complete operation
    while ((!(TWCR & (1<<TWINT))) && (i < num_tries))
		i++;
	return (i<num_tries);
}

void i2cSendStart(void)
{   // Send START or REPEATED START
	// Make sure that there is at least 1.3 microseconds between stop and start
	delayMicroseconds(2);

	pinMode(SDA,OUTPUT);
	// send start condition
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	i2cWaitForComplete();	// could separate this, to allow computation while waiting
	if (!START_OK) {i2cError("Start not acked");}
}

void i2cSendRepeatedStart(void)
{   // Send START or REPEATED START
	// Make sure that there is at least 1.3 microseconds between stop and start
	delayMicroseconds(2);
		
	pinMode(SDA,OUTPUT);
	// send start condition
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	i2cWaitForComplete();	// could separate this, to allow computation while waiting
	if (!REPEATED_START_OK) {i2cError("Repeated Start not acked");}
}
		

void i2cSendStop(void)
{
	// transmit stop condition
	pinMode(SDA,OUTPUT);
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
	// wait for STOP to complete
	while (TWCR & (1<<TWSTO)) {}
}


void i2cSendByte(uint8_t data)
{
	// delay(1);
	pinMode(SDA,OUTPUT);
	while (!i2cWaitForComplete())
	{   i2cError("i2c not ready for sending");
		delay(2);
	}

	// save data to the TWDR
	TWDR = data;
	// begin send
	TWCR = (1<<TWINT)|(1<<TWEN);
	i2cWaitForComplete();	// could separate this, to allow computation while waiting
}

void i2cSendWriteAddress(uint8_t  i2c_7bit_address)
{
	i2cSendByte((i2c_7bit_address<<1));	// write mode  
	if(!WRITE_ADDRESS_OK) i2cError("Write address not acked");
}

void i2cSendReadAddress(uint8_t  i2c_7bit_address)
{
	i2cSendByte((i2c_7bit_address<<1)|0x01);	// read mode	
	if(!READ_ADDRESS_OK) i2cError("Read address not acked");
}

void i2cSendData(uint8_t data)
{   
	i2cSendByte(data);
	if (!WRITE_DATA_OK) {i2cError("Write byte not acked");}
}
	
void i2cReceiveByte(uint8_t ackFlag)
{
	pinMode(SDA,INPUT);
	// begin receive over i2c
	if( ackFlag )
	{	// ackFlag = TRUE: ACK the recevied data
		TWCR = (TWCR&TWCR_CMD_MASK) |_BV(TWINT) |_BV(TWEA);
	}
	else
	{
		// ackFlag = FALSE: NACK the recevied data
		TWCR = (TWCR&TWCR_CMD_MASK) |_BV(TWINT);
	}
	i2cWaitForComplete();	// could separate this, to allow computation while waiting
	if (!READ_DATA_OK) {i2cError("Read byte not acked");}

}

uint8_t i2cGetReceivedByte(void)
{
	// retieve received data byte from i2c TWDR
	return( TWDR );
}

uint8_t i2cGetStatus(void)
{
	// retieve current i2c status from i2c TWSR
	return( TWSR );
}

// read a single byte from address and return it as a byte
uint8_t i2cReadRegister(uint8_t i2c_7bit_address, uint8_t address)
{
  uint8_t data;
  
  i2cSendStart();
  i2cSendWriteAddress(i2c_7bit_address);
  i2cSendData(address);	// write register address  
  i2cSendRepeatedStart();		// repeated start
  i2cSendReadAddress(i2c_7bit_address);
  i2cReceiveByte(0);  
  data = i2cGetReceivedByte();	// Get result
  i2cSendStop();

  cbi(TWCR, TWEN);	// Disable TWI
  sbi(TWCR, TWEN);	// Enable TWI
  
  return data;
}

// Read num_to_read registers sequentially, starting at address 
//   into the dest byte arra 
void i2cReadRegisters(uint8_t i2c_7bit_address, uint8_t address, 
	uint8_t num_to_read, uint8_t * dest)
{
  i2cSendStart();  
  i2cSendWriteAddress(i2c_7bit_address);
  i2cSendData(address);	// write register address  	
  i2cSendRepeatedStart();		// repeated start
  i2cSendReadAddress(i2c_7bit_address);
  int j;
  for (j=0; j<num_to_read-1; j++)
  {
    i2cReceiveByte(1);
	dest[j] = i2cGetReceivedByte();	// Get data
  }
  i2cReceiveByte(0);
  dest[j] = i2cGetReceivedByte();	// Get data
  i2cSendStop();
  
  cbi(TWCR, TWEN);	// Disable TWI
  sbi(TWCR, TWEN);	// Enable TWI
}

/* Writes a single byte (data) into address */
void i2cWriteRegister(uint8_t i2c_7bit_address, uint8_t address, uint8_t data)
{
  i2cSendStart();  
  i2cSendWriteAddress(i2c_7bit_address);
  i2cSendData(address);	// write register address  
  i2cSendData(data);  
  i2cSendStop();
}

/* Writes num_to_send bytes of data starting at address */
void i2cWriteRegisters(uint8_t i2c_7bit_address, uint8_t address, uint8_t num_to_send, const uint8_t *data)
{
  i2cSendStart();  
  i2cSendWriteAddress(i2c_7bit_address);
  i2cSendData(address);	// write register address  
  for (int j=0; j<num_to_send; j++)
  {   i2cSendData(data[j]);
  }
  i2cSendStop();
}
