// tinyCylon2.c
// revised firmware for tinyCylon LED scanner
// written by dale wheat - 18 november 2008
// based on behavior of original tinyCylon firmware

// notes:

// device = ATtiny13A
// clock = 128 KHz internal RC oscillator
// max ISP frequency ~20 KHz
// brown-out detect = 1.8 V

#define F_CPU 1000000UL  // 1 MHz
//#define F_CPU 14.7456E6
#include <util/delay.h>


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

// These registers are available on the ATtiny13A but not the original ATtiny13

// Brown out detector control register

#define BODCR _SFR_IO8(0x30)
#define BODSE 0
#define BODS 1

// Power reduction register

#define PRR _SFR_IO8(0x3C)
#define PRADC 0
#define PRTIM0 1

typedef enum {
	MODE_0,  // Changes every per tilt, on for 2 seconds per tilt.
	MODE_1,  // Changes every 5 seconds
	MODE_2,  // Changes per tilt
	MODE_MAX // off
} MODE;

volatile unsigned char icount;    // interrupt flag
unsigned char ibit;               // index into bit patterns
const unsigned char bbits[] = {   // bit patterns
   // 76543210    
	0b00011010,
	0b00010011,
	0b00001011
};

// Function Prototypes
void GoToSleep(unsigned char TiltFlag);
void SetTilt(void);
void Reset(void);

// Functional dependent defines
#define MAXBLINK 5

volatile MODE mode __attribute__ ((section (".noinit")));

///////////////////////////////////////////////////////////////////////////////
// init() - initialize everything
// note:  this "function" is in section .init3 so it is executed before main()
///////////////////////////////////////////////////////////////////////////////

void init(void) __attribute__ ((naked, section(".init3")));
void init(void) {
	// determine cause of device reset;  act accordingly
	if(bit_is_set(MCUSR, PORF)) {
		mode = MODE_0; // power on!
	} else if(bit_is_set(MCUSR, EXTRF)) {
		mode++; // advance mode
		if(mode > MODE_MAX) {
			mode = MODE_0; // reset mode
		}
	}

	// initialize ATtiny13 input & output port

	// PORTB

	//	PB5		1		PCINT5/-RESET/ADC0/dW		MODE advance pushbutton
	//	PB3		2		PCINT3/CLKI/ADC3			D4 output, active low
	//	PB4		3		PCINT4/ADC2					D5 output, active low
    //  GND     4       GROUND

	//	PB0		5		MOSI/AIN0/OC0A/PCINT0		D1 output, active low
	//	PB1		6		MISO/AIN1/OC0B/INT0/PCINT1	D2 input, tilt
 	//	PB2		7		SCK/ADC1/T0/PCINT2			D3 output, n/c
	//  VCC     8       POWER

	Reset();
	SetTilt();
}

void Reset(void) {
	// turn off unused peripherals to save power
	MCUSR = 0; // reset bits
	ACSR = 1<<ACD; // disable analog comparator
	DIDR0 = 1<<ADC3D | 1<<ADC2D | 1<<ADC1D | 1<<ADC0D | 0<<AIN1D | 1<<AIN0D; // disable digital inputs
	PORTB = 0<<PORTB5 | 1<<PORTB4 | 1<<PORTB3 | 1<<PORTB2 | 0<<PORTB1 | 1<<PORTB0;
	DDRB = 0<<DDB5 | 1<<DDB4 | 1<<DDB3 | 1<<DDB2 | 0<<DDB1 | 1<<DDB0;
}

///////////////////////////////////////////////////////////////////////////////
// main() - main program function
///////////////////////////////////////////////////////////////////////////////
int main(void) {
icount = MAXBLINK;
ibit = 3; 
while (1) {
 if (++ibit >= 3) ibit=0;
 PORTB = bbits[ibit];
 switch (mode) {
  case MODE_0:  _delay_ms(100);
	            break;
  case MODE_1:  _delay_ms(1000);
			    break;
  case MODE_2:  _delay_ms(10000);
	            break;
  case MODE_MAX: GoToSleep(0);
  }
 while (!icount) GoToSleep(1);
 icount--;
 }
return 0;
}

///////////////////////////////////////////////////////////////////////////////
// sleep() - puts cpu to sleep
///////////////////////////////////////////////////////////////////////////////
void GoToSleep(unsigned char TiltFlag) {
	PORTB = 	0b00011011; // all LEDs off
	// deepest sleep mode
	cli(); // disable interrupts
	PRR = 1<<PRTIM0 | 1<<PRADC; // power down timer/counter0 & ADC
	BODCR = 1<<BODS | 1<<BODSE; // enable BOD disable during sleep, step 1
	BODCR = 1<<BODS | 0<<BODSE; // step 2
	
	// Enable tilt switch interrupt
	if (TiltFlag) SetTilt();
	else MCUCR = 1<<PUD | 1<<SE | 1<<SM1 | 0<<SM0 | 0<<ISC01 | 0<<ISC00; // select "power down" mode
		
	// go to sleep to save power
	asm("sleep"); 

	// restart
	Reset();
}

void SetTilt() {
 MCUCR = 1 << ISC01;  //set INT0 as falling edge trigger     
 GIMSK = 1 << INT0;   //enable INTO in global interrupt mask
 sei(); // enable global interrupts
}



///////////////////////////////////////////////////////////////////////////////
// timer/counter0 overflow interrupt handler
///////////////////////////////////////////////////////////////////////////////

ISR(INT0_vect) {
	icount = MAXBLINK;
}

///////////////////////////////////////////////////////////////////////////////

// [end-of-file]
