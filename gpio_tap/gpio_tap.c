// See LICENSE for license details.

// This is the program which ships on the HiFive1
// board, executing out of SPI Flash at 0x20400000.

#include <stdint.h>
#include "platform.h"

#ifndef _SIFIVE_HIFIVE1_H
#error "'led_fade' is designed to run on HiFive1 and/or E300 Arty Dev Kit."
#endif

static const char led_msg[] = "\a\n\r\n\r\
55555555555555555555555555555555555555555555555\n\r\
5555555 Are the LEDs Changing? [y/n]  555555555\n\r\
55555555555555555555555555555555555555555555555\n\r";

static const char sifive_msg[] = "\n\r\
\n\r\
                SIFIVE, INC.\n\r\
\n\r\
         5555555555555555555555555\n\r\
        5555                   5555\n\r\
       5555                     5555\n\r\
      5555                       5555\n\r\
     5555       5555555555555555555555\n\r\
    5555       555555555555555555555555\n\r\
   5555                             5555\n\r\
  5555                               5555\n\r\
 5555                                 5555\n\r\
5555555555555555555555555555          55555\n\r\
 55555           555555555           55555\n\r\
   55555           55555           55555\n\r\
     55555           5           55555\n\r\
       55555                   55555\n\r\
         55555               55555\n\r\
           55555           55555\n\r\
             55555       55555\n\r\
               55555   55555\n\r\
                 555555555\n\r\
                   55555\n\r\
                     5\n\r\
\n\r";

static void _putc(char c) {
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


static void _puts(const char * s) {
  while (*s != '\0'){
    _putc(*s++);
  }
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
  //_puts("Config String:\n\r");
  //_puts(*((const char **) 0x100C));
  //_puts("\n\r");
  //_puts(led_msg);
  
  uint16_t r=0xFF;
  uint16_t g=0;
  uint16_t b=0;
  char c = 0;
  int gpval=0;
  
  // Set up RGB PWM
  
  PWM1_REG(PWM_CFG)   = 0;
  // To balance the power consumption, make one left, one right, and one center aligned.
  PWM1_REG(PWM_CFG)   = (PWM_CFG_ENALWAYS) | (PWM_CFG_CMP2CENTER);
  PWM1_REG(PWM_COUNT) = 0;
  
  // Period is approximately 244 Hz
  // the LEDs are intentionally left somewhat dim, 
  // as the full brightness can be painful to look at.
  PWM1_REG(PWM_CMP0)  = 0;

  //Set up GPIO23
  GPIO_REG(GPIO_IOF_SEL)    &= 0xFF7FFFFF;
  GPIO_REG(GPIO_IOF_EN)     &= 0xFF7FFFFF;
  GPIO_REG(GPIO_INPUT_EN)   &= 0xFF7FFFFF;
  GPIO_REG(GPIO_OUTPUT_EN)  |= 0x00800000;
  GPIO_REG(GPIO_PULLUP_EN)  &= 0xFF7FFFFF;
  GPIO_REG(GPIO_DRIVE)      &= 0xFF7FFFFF;
  GPIO_REG(GPIO_OUTPUT_VAL) &= 0xFF7FFFFF;

  GPIO_REG(GPIO_IOF_SEL)    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
  GPIO_REG(GPIO_IOF_EN )    |= ( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET) | (1 << RED_LED_OFFSET));
  GPIO_REG(GPIO_OUTPUT_XOR) &= ~( (1 << GREEN_LED_OFFSET) | (1 << BLUE_LED_OFFSET));
  GPIO_REG(GPIO_OUTPUT_XOR) |= (1 << RED_LED_OFFSET);

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
  
    // Check for user input
    //    if (c == 0){
      if (_getc(&c) != 0){
	gpval=(gpval+1)%6;
	_puts("GPIO Setting ");
        _putc('0'+gpval);
	_putc(':');
	if ( gpval < 4 ) {
          GPIO_REG(GPIO_INPUT_EN)   &= 0xFF7FFFFF;
  	  GPIO_REG(GPIO_OUTPUT_EN)  |= 0x00800000;

	  if(gpval%2) {
	    _puts(" Value=1");
	    GPIO_REG(GPIO_OUTPUT_VAL) |= 0x00800000;
	  } else {
	    _puts(" Value=0");
	    GPIO_REG(GPIO_OUTPUT_VAL) &= 0xFF7FFFFF;
	  }
	  if(gpval/2%2) {
	    _puts(" HIGH Drive");
	    GPIO_REG(GPIO_DRIVE)      |= 0x00800000;
	  } else {
	    _puts(" LOW Drive");
	    GPIO_REG(GPIO_DRIVE)   &= 0xFF7FFFFF;
	  } 
	} else {
          GPIO_REG(GPIO_INPUT_EN)   |= 0x00800000;
  	  GPIO_REG(GPIO_OUTPUT_EN)  &= 0xFF7FFFFF;
	  _puts(" Input Mode");
	  if(gpval%2) {
	    _puts(" Pullup=1");
	    GPIO_REG(GPIO_PULLUP_EN)  |= 0x00800000;
	  } else {
    	    _puts(" Pullup=0");
    	    GPIO_REG(GPIO_PULLUP_EN)  &= 0xFF7FFFFF;
	  }

	}
        _puts("\n\r");
      }
      //    }
  }
}
