
/*
  Jesper Hansen <jesperh@telia.com>
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation, 
  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * 2020-MAR-29, xiaolaba
 * try and port to uses ATtiny13
 * compiler : Arduino IDE 1.8.12, or avr-gcc 4.3.3
 */



/*
	Simple Digital Guitar Tuner
	---------------------------
	version 1.0		2001-02-12	jesper
	
	PIN assignements on the 2323
	PB0		High LED
	PB1		Input pin
	PB2		Low LED
*/

#define LED_HI PB0
#define MIC_IN PB1
#define LED_LO PB2

#include <avr/io.h>
//#include <signal.h>
#include <avr/interrupt.h>

// enable sbi/cbi instruction & support, xiaolaba 2020-MAR-29
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

//#define F_CPU				11059200			// CPU clock frequency, AT90S2323
#define F_CPU         9600000L      // CPU clock frequency, Attiny13
#define PRESCALER			64					// CPU prescaler value



#define BASE_FREQUENCY		(F_CPU/PRESCALER)	// counter frequency	

#define TUNING_FORK_A		440.0				// "base" A
#define NOTE_DIFF			1.05946309436		// the 12'th root of 2





#define E_STRING	164.81						// top string (1)
#define A_STRING	220.00
#define D_STRING	293.66
#define G_STRING	391.99
#define B_STRING	493.88				
#define EH_STRING	659.26						// bottom string (6)


// The guitar note span
//  # # #  # #  # # #  # #
//EF G A BC D EF G A BC D E
//1    2    3    4 * 5    6
//

unsigned int Center_Count[] =
{
	BASE_FREQUENCY/EH_STRING,			// High E
	BASE_FREQUENCY/B_STRING,			// B
	BASE_FREQUENCY/G_STRING,			// G
	BASE_FREQUENCY/D_STRING,			// D
	BASE_FREQUENCY/A_STRING,			// A
	BASE_FREQUENCY/E_STRING,			// Low E
};

unsigned int Transition_Count[] =
{
	BASE_FREQUENCY/(B_STRING+(EH_STRING-B_STRING)/2),	// E to B
	BASE_FREQUENCY/(G_STRING+(B_STRING-G_STRING)/2),		// B to G
	BASE_FREQUENCY/(D_STRING+(G_STRING-D_STRING)/2),		// G to D
	BASE_FREQUENCY/(A_STRING+(D_STRING-A_STRING)/2),		// D to A
	BASE_FREQUENCY/(E_STRING+(A_STRING-E_STRING)/2),		// A to E
};




volatile unsigned char count_hi;	// overflow accumulator

//
//timer 0 overflow interrupt
//
/*
SIGNAL(SIG_OVERFLOW0)	// AT90S2323
{
	count_hi++;						// increment overflow count
}
*/

ISR (TIM0_OVF_vect)    // Timer0 ISR, Attiny13, gcc-4.3.3
{
  count_hi++;           // increment overflow count
}


//----------------------------------------------------------------------------
// Main Lupe
//----------------------------------------------------------------------------


int main(void) 
{
	unsigned int i; 
	unsigned int count;
	
 //------------------------------
 // Initialize 
 //------------------------------

// xiaolaba, 2020-MAR-29
// https://www.nongnu.org/avr-libc/user-manual/group__avr__sfr.html
// The cbi() & sbi() is defined in hardware/arduino/avr/cores/arduino/wiring_private.h, Arduino IDE 1.8.12
// #define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
// #define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
// deprecated sbi/cbi macros, bring it back

  cbi(DDRB, 1);			// PB1 is input
  DDRB &= ~(_BV(MIC_IN)); // PB1 is input   
 
  cbi(PORTB, 1);		// no pullups active
  PORTB&= ~(_BV(MIC_IN)); // no pullups active
  	
  sbi(DDRB, 0);			// PB0 is ouput, High LED
  DDRB |= _BV(LED_HI);  // PB0 is ouput, High LED   
	
  sbi(DDRB, 2);			// PB2 is ouput, Low LED
  DDRB |= _BV(LED_LO);  // PB2 is ouput, Low LE

/*
  // TCCR0, addrees 0x33/0x53 for AT90S2323/4323
  outp(0x03,TCCR0);		// set prescaler to f/64 (172.8 kHz @ 11.0592 MHz)
*/
// TCCR0B, Attiny13
  TCCR0B = 0x03;   // set prescaler to f/64 (150 kHz @ 9.6 MHz)

  //sbi(TIMSK,TOIE0);		// enable interrupt on timer overflow, AT90S2323
  TIMSK0 = 1<TOIE0;    // Attiny13, enable interrupt on timer overflow

	asm volatile ("sei");	// global interrupt enable

 //----------------------------------------------------------------------------
 // Let things loose
 //----------------------------------------------------------------------------                        


	while (1)								// loop forever
	{
		count = 0;							// clear sample count
		loop_until_bit_is_set(PINB,1);		// wait for something to happen
    loop_until_bit_is_set(PINB,MIC_IN);    // wait for something to happen

		// got a high edge
		// start sampling
		
		//outp(0,TCNT0);						// clear counter
    TCNT0 =0;            // clear counter		 
		count_hi = 0;						// clear hi count

		
		// sample loop
		
		for (i=0;i<32;i++)
		{
			while (bit_is_set(PINB,1))		// ignore hi->lo edge transitions
				if (count_hi > 80)			// skip if no edge is seen within
					break;					// a reasonable time

			while (bit_is_clear(PINB,1))	// wait for lo->hi edge
				if (count_hi > 80)			// skip if no edge is seen within
					break;					// a reasonable time

//			count += (count_hi << 8) + inp(TCNT0); // get counter value
//			outp(0,TCNT0);					// clear counter		 
      count += (count_hi << 8) + TCNT0; // get counter value
      TCNT0 =0;          // clear counter     


			if (count_hi > 80)				// skip if counter has accumulated a
				break;						// too high value

			count_hi = 0;					// clear hi count
		}




		// initially turn off both leds
		sbi(PORTB,0);
		sbi(PORTB,2);
    PORTB |= _BV(LED_HI);  
    PORTB |= _BV(LED_LO);
    //PORTB = (1<LED_HI) + (1<LED_LO); same result as above two line
    
		if (count_hi <= 80)					// if count is reasonable
		{

			count = count >> 5;				// average accumulated count by dividing with 32
	
			// now we have to find the correct string
						
			// go through transition frequencies
			
			for (i=0;i<sizeof(Transition_Count)/sizeof(Transition_Count[0]);i++)
			{
				if (count < Transition_Count[i])	// stop if lower than this transition count
					break;
			}				
			
			// i now holds the string index
			
			// check the count for a match, allowing
			// 1 extra count "hysteresis" to avoid too 
			// much LED flickering
			
			if (count-1 <= Center_Count[i])			// if count <= this string count
				cbi(PORTB,0);						// light "Too High" LED
        PORTB &= ~(_BV(LED_HI)); 
	
			if (count+1 >= Center_Count[i])			// if count >= this string count
				cbi(PORTB,2);						// light "Too Low" LED
        PORTB &= ~(_BV(LED_LO)); 
		}
	}


} 
