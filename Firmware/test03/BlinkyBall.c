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

unsigned int loop;               // loop counter 
volatile unsigned int interrupt; // interrupt flag
unsigned int reset;              // tracks reset to next color change
unsigned int max;                // where to reset to

volatile MODE mode __attribute__ ((section (".noinit")));

///////////////////////////////////////////////////////////////////////////////
// init() - initialize everything
// note:  this "function" is in section .init3 so it is executed before main()
///////////////////////////////////////////////////////////////////////////////

void init(void) __attribute__ ((naked, section(".init3")));
void init(void) {

	// turn off unused peripherals to save power

	ACSR = 1<<ACD; // disable analog comparator
	DIDR0 = 1<<ADC3D | 1<<ADC2D | 1<<ADC1D | 1<<ADC0D | 0<<AIN1D | 1<<AIN0D; // disable digital inputs

	// determine cause of device reset;  act accordingly

	if(bit_is_set(MCUSR, PORF)) {
		mode = MODE_0; // power on!
	} else if(bit_is_set(MCUSR, EXTRF)) {
		mode++; // advance mode
		if(mode > MODE_MAX) {
			mode = MODE_0; // reset mode
		}
	}

	MCUSR = 0; // reset bits

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

	PORTB = 0<<PORTB5 | 1<<PORTB4 | 1<<PORTB3 | 1<<PORTB2 | 0<<PORTB1 | 1<<PORTB0;
	DDRB = 0<<DDB5 | 1<<DDB4 | 1<<DDB3 | 1<<DDB2 | 0<<DDB1 | 1<<DDB0;

	// initialize ATtiny13 timer/counter

	TCCR0B = 0<<FOC0A | 0<<FOC0B | 0<<WGM02 | 0<<CS02 | 0<<CS01 | 1<<CS00;
	TIMSK0 = 0<<OCIE0B | 0<<OCIE0A | 1<<TOIE0; // interrupts

	// Enable tilt switch interrupt
	MCUCR = 1 << ISC01;  //set INT0 as falling edge trigger     
    GIMSK = 1 << INT0;   //enable INTO in global interrupt mask

	sei(); // enable global interrupts
}

///////////////////////////////////////////////////////////////////////////////
// timing & delay functions
///////////////////////////////////////////////////////////////////////////////

volatile unsigned char downcounter;

void delay(unsigned char n) {

	downcounter = n;

	while(downcounter) {
		MCUCR = 1<<PUD | 1<<SE | 0<<SM1 | 0<<SM0 | 0<<ISC01 | 0<<ISC00; // idle mode
		asm("sleep"); // go to sleep to save power
	}
}

///////////////////////////////////////////////////////////////////////////////
// pseudorandom number generator
///////////////////////////////////////////////////////////////////////////////

unsigned int prand(void) {

    static unsigned int prand_value = 0xDA1E; // randomly seeded ;)
    
    prand_value = (prand_value >> 1) ^ (-(prand_value & 1) & 0xd001);

    return prand_value;
}

///////////////////////////////////////////////////////////////////////////////
// main() - main program function
///////////////////////////////////////////////////////////////////////////////



int main(void) {

interrupt = 0;

const unsigned char bbits[] = {
	0b00000010,
	0b00000011,
	0b00001010,
	0b00010010,
	0b00001011,
	0b00010011,
	0b00011010,
	0b00011011
	};

unsigned char i=sizeof(bbits);

 while (1) {
  if (++i >= sizeof(bbits)) i=0;
  PORTB = bbits[i];
  _delay_ms(250);
  while (!interrupt) sleep();
  interrupt--;
 }

return 0;
}

void sleep() {
	PORTB = 0b00011011; // all LEDs off
	// deepest sleep mode
	cli(); // disable interrupts
	PRR = 1<<PRTIM0 | 1<<PRADC; // power down timer/counter0 & ADC
	BODCR = 1<<BODS | 1<<BODSE; // enable BOD disable during sleep, step 1
	BODCR = 1<<BODS | 0<<BODSE; // step 2
	MCUCR = 1<<PUD | 1<<SE | 1<<SM1 | 0<<SM0 | 0<<ISC01 | 0<<ISC00; // select "power down" mode
	asm("sleep"); // go to sleep to save power
}

///////////////////////////////////////////////////////////////////////////////
// timer/counter0 overflow interrupt handler
///////////////////////////////////////////////////////////////////////////////

ISR(TIM0_OVF_vect) {

	downcounter--; // decrement downcounter for delay functions
}

ISR(INT0_vect) {
	interrupt = 25;
}

///////////////////////////////////////////////////////////////////////////////

// [end-of-file]