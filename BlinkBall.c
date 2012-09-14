#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define LOOPS 10   // Number of Loops per tilt
#define DELAY 100  // Number of ms to delay per loop

volatile int loop = LOOPS;

void main() {
 setup();
 for (;;) {loop()};
}

setup{
 DDRB  = 0b0001 1100; // output 
 PORTB = 0b0001 1100; // all high
 MCUCR = 1 << ISC01;  //set INT0 as falling edge trigger     
 GIMSK = 1 << INT0;   //enable INTO in global interrupt mask
 SEI();
}

loop{
 if (loop) {
  while (loop--) {
   ADCSRA |= (1<<ADEN) | (1<<ADPS0) | (1<<ADPS1);  // ENABLE ADC/64
   //ADCSRB = 0X00; // FREE RUNNING
   ADMUX = 0X21; // VCC REF, LEFT ADJ, ADC1
   //DIDR0 = 0X04; DISABLE INPUT ON PB2
   ADCSRA |= (1<<ADSC);                // START CONVERSION
   WHILE (ADCSRA & 0X40) {};           // WAIT FOR FINISH
   PORTB |= (ADCH & 0b0000 0111) << 2;
   delay_ms(DELAY);
  }
 }
 ADCSRA |= (0<<ADEN); // TURN OFF ADC
 set_sleep_mode(SLEEP_MODE_PWR_DOWN); // SET SLEEP MODE, DEEPEST
 sleep_mode(); // GO TO SLEEP
}

ISR(INT0_vect) {
 // increase tick count
 // the more it moves, the longer it stays on
 loop++;
}
