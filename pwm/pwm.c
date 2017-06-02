// See LICENSE for license details.

// This is the program which ships on the HiFive1
// board, executing out of SPI Flash at 0x20400000.

#include <stdint.h>
#include "platform.h"
#include <stdio.h>

#ifndef _SIFIVE_HIFIVE1_H
#error "'led_fade' is designed to run on HiFive1 and/or E300 Arty Dev Kit."
#endif

#define true 1
#define false 0

static char mycr[] = "\r\n";

static char sfprompt[] = "SiFive>>";

static char sifive_msg[] = "\r\n\
\r\n\
                SIFIVE, INC.\r\n\
\r\n\
         5555555555555555555555555\r\n\
        5555                   5555\r\n\
       5555                     5555\r\n\
      5555                       5555\r\n\
     5555       5555555555555555555555\r\n\
    5555       555555555555555555555555\r\n\
   5555                             5555\r\n\
  5555                               5555\r\n\
 5555                                 5555\r\n\
5555555555555555555555555555          55555\r\n\
 55555           555555555           55555\r\n\
   55555           55555           55555\r\n\
     55555           5           55555\r\n\
       55555                   55555\r\n\
         55555               55555\r\n\
           55555           55555\r\n\
             55555       55555\r\n\
               55555   55555\r\n\
                 555555555\r\n\
                   55555\r\n\
                     5\r\n\
\r\n";

static void _putc( char c) {
  while ((int32_t) UART0_REG(UART_REG_TXFIFO) < 0);
  UART0_REG(UART_REG_TXFIFO) = c;
}

int _getc(char * c){
  int32_t val = (int32_t) UART0_REG(UART_REG_RXFIFO);
  if (val > 0) {
    *c =  val & 0xFF;
    return 1;
  }
  return 0;
}


static void _puts( char * s) {
  while (*s != '\0') _putc(*s++);
}


int main (void){

  // Run off 16 MHz Crystal for accuracy.
  PRCI_REG(PRCI_PLLCFG) = (PLL_REFSEL(1) | PLL_BYPASS(1));
  PRCI_REG(PRCI_PLLCFG) |= (PLL_SEL(1));
  // Turn off HFROSC to save power
  PRCI_REG(PRCI_HFROSCCFG) = 0;
  
  // Configure UART to print
  GPIO_REG(GPIO_OUTPUT_VAL) |= IOF0_UART0_MASK;
  GPIO_REG(GPIO_OUTPUT_EN)  |= IOF0_UART0_MASK;
  GPIO_REG(GPIO_IOF_SEL)    &= ~IOF0_UART0_MASK;
  GPIO_REG(GPIO_IOF_EN)     |= IOF0_UART0_MASK;

  // 115200 Baud Rate
  //  UART0_REG(UART_REG_DIV) = 138;
  UART0_REG(UART_REG_DIV) = 138;
  UART0_REG(UART_REG_TXCTRL) = UART_TXEN;
  UART0_REG(UART_REG_RXCTRL) = UART_RXEN;

  // Wait a bit to avoid corruption on the UART.
  // (In some cases, switching to the IOF can lead
  // to output glitches, so need to let the UART
  // reciever time out and resynchronize to the real 
  // start of the stream.
  volatile int i=0;
  while(i < 10000){i++;}

  _puts(sifive_msg);
  _puts(sfprompt);
  
  uint16_t r=0xFF;
  uint16_t g=0;
  uint16_t b=0;
  char c = 0;
  
  // Set up RGB PWM
  PWM1_REG(PWM_CFG)   = 0;
  // To balance the power consumption, make one left, one right, and one center aligned.
  PWM1_REG(PWM_CFG)   = (PWM_CFG_ENALWAYS) | (PWM_CFG_CMP2CENTER);
  PWM1_REG(PWM_COUNT) = 0;

  // Set up PWM2
  // PWM0 corresponds to pins 9,10 & 11 ( comparators 1,2,3 ) on HiFive1
  // PWM2 corresponds to pins 17,18,19 ( comparators 1,2,3 ) on HiFive1
  PWM2_REG(PWM_CFG)   = 0; // Not strictly needed

  // PWM_CFG_ZEROCMP resets the PWM counter when PWM_CMP0 is triggered
  PWM2_REG(PWM_CFG) = (PWM_CFG_ENALWAYS) | (PWM_CFG_ZEROCMP);
  PWM2_REG(PWM_CFG) |= (0x00 & PWM_CFG_SCALE); // PWM_SCALE is 4 Bits Wide

  PWM2_REG(PWM_CMP3)  = 0x08; // CMP1 must be < CMP0
  PWM2_REG(PWM_CMP2)  = 0x08; // CMP1 must be < CMP0
  PWM2_REG(PWM_CMP1)  = 0x08; // CMP1 must be < CMP0
  PWM2_REG(PWM_CMP0)  = 0x0F; // This sets a full cmp cycle.  0x0F is 1MHz

  GPIO_REG(GPIO_IOF_SEL)    |= 0x3C00; // GPIO10-13 SEL
  GPIO_REG(GPIO_IOF_EN)    |= 0x3C00; // GPIO10-13 SEL
  GPIO_REG(GPIO_OUTPUT_XOR)    |= 0x3C00; // GPIO0-3 SEL
  
  
  // Period is approximately 244 Hz
  // the LEDs are intentionally left somewhat dim, 
  // as the full brightness can be painful to look at.
  PWM1_REG(PWM_CMP0)  = 0;

  GPIO_REG(GPIO_IOF_SEL)    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
  GPIO_REG(GPIO_IOF_EN )    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
  GPIO_REG(GPIO_OUTPUT_XOR) &= ~( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET));
  GPIO_REG(GPIO_OUTPUT_XOR) |= (1 << RED_LED_OFFSET);

  char inputLine[255];
  inputLine[0]='\0';
  char *inputPtr=inputLine;

  while(1){
    volatile uint64_t *  now = (volatile uint64_t*)(CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t then = *now + 100;
    while (*now < then) { }
  
    if(r > 0 && b == 0){
      r--;
      g++;
    }
    if(g > 0 && r == 0){
      g--;
      b++;
    }
    if(b > 0 && g == 0){
      r++;
      b--;
    }
    
    uint32_t G = g;
    uint32_t R = r;
    uint32_t B = b;
    
    PWM1_REG(PWM_CMP1)  = G << 4;            // PWM is low on the left, GPIO is low on the left side, LED is ON on the left.
    PWM1_REG(PWM_CMP2)  = (B << 1) << 4;     // PWM is high on the middle, GPIO is low in the middle, LED is ON in the middle.
    PWM1_REG(PWM_CMP3)  = 0xFFFF - (R << 4); // PWM is low on the left, GPIO is low on the right, LED is on on the right.

    PWM2_REG(PWM_CMP3)  = 0xFFFF - (R << 4); // PWM is low on the left, GPIO is low on the right, LED is on on the right.
  
    // Check for user input
    if (_getc(&c) != 0){

      volatile int k=0;

      switch(c){
  	  case '\n':
	  case '\r':
	    inputPtr=inputLine;
   	    _puts(mycr);
	    _puts(inputLine);
       	    _puts(mycr);
	    _puts(sfprompt);
	    inputLine[0]='\0';
	    break;
	  default:
	      *inputPtr=c;
     	      _putc(c);
	      inputPtr++;
	      *inputPtr='\0';

	      if(c>='0' && c<='9') {
		PWM2_REG(PWM_CMP1)  = c-'0';
	      }
	      break;
      }
    }
  }
}

