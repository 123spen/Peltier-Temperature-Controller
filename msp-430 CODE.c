/*-----------------------------------------------------------------------
 desin_lab_Taz_Spencer.c
 Written by Spencer McDermott and Taz Colanhelo
 Date: 2017 April 4
-----------------------------------------------------------------------*/


#include "io430.h"
//--------------------------------
 unsigned char samples[2];
 unsigned char countSend;
 unsigned int sample;
 
          long unsigned int cycle = 0; 
          float temperature; 
          float setTemp = 25;
          float errorInt = 0;          
          float errorPast[20];
         
          
//---------------------------------

   
          
void sampler(void)
{
for(int count = 0; count < sizeof samples; count++)
  {  
     //take ADC vaule place the fisrt 8-bit in the char/byte arrey then puts the 2 overflow bit in the next location in the char/byte arrey 
     sample = ADC10MEM;
     samples[count] =  sample;
     samples[++count] =  sample>>8;
  }
}

void calculateTemp() // convert ADC step into temperature
{
  temperature = 6.57026007861345E-5*(sample)*(sample) + 0.015136513090065*sample - 15.6012551275939;  
}

float errorP()  //proportional error
{
  return setTemp - temperature;
}

void errorArray() //makes errorPast a dynamic array filled beginning to end for the last 20 temperature errors
{
 for(int count = 0; count < 19; count++)
    {
    errorPast[19 - count] = errorPast[18 - count];
    }
    errorPast[0] = errorP();
}

float errorD() //derivative error: takes the mean slope of 20 points were dx = 1 cycle
{   
 if(cycle > 19)
  { 
   float errorDSum = 0;
   
    for(int count = 0; count < 19; count++)
    {
    errorDSum += (errorPast[19 - count] - errorPast[18 - count]) / 2;
    }
     return errorDSum/19;
  }
  else
  {
    return 0;
  }
}

float errorI()  //integral error: use trapezoidal rule were dx = 1 cycle
{
  errorInt += (errorPast[1] + errorPast[0]) / 2;
  
  
  //Anti-Windup Control for integral error at cap of abs(500)
   if(500 < (int)errorInt)
   {
    errorInt = 500;
   } 
   else if(-500 > (int)errorInt)
   {
     errorInt = -500;
   }  
   return errorInt; 
}
int PIDcontroller()
{
  int result = 0;
  //PID constants
  float KP = 0.027;
  float KI = 0.00003;
  float KD = 1000;
 
  errorArray();
  //calculate the amount to increase/decrease PWM
  result = (long int) (KP * errorP() + KI * errorI() + KD *  errorD());
  
    cycle++;
    
    return result;
 }
void PWM_control(result)
{
    // if PWM at 100% duty cycle 0xffff do not increase TA1CCR0(will cause error; set TA1CCR0 = result -1)
    if( (65535 < ((long int)TA1CCR0 + (long int)result) && (P2OUT_bit.P0 == 1))  || ((65535 < ((long int)TA1CCR0 - (long int)result)) && (P2OUT_bit.P0 == 0))) 
  {
        while(1)
       {
         if(TA0R == 0)
         {
           TA1CCR0 = 0xFFFF;
           break;
         }
       }
  }
  else 
  { 
    //if TA1CCR0 + result is = to 0 increase result (so error does not occur: read's 0 as 0xFFFF)
    if(TA1CCR0 + result == 0)  // if in cooling mode
    {
       result++;
    }
    // if the result of result and TA1CCR0 dictates that the PWM is/should be in heating mode and the PMW is not to have a duty cycle less then 0% if in heating mode
    if(((long int)TA1CCR0 + (long int)result > 0 && (P2OUT_bit.P0 == 1)) || ((long int)(result - (long int)TA1CCR0)  > 0 && (P2OUT_bit.P0 == 0)) )
    {
     if(P2OUT_bit.P0 == 0) // if in cooling mode
     {
       while(1)
       {
         if(TA0R == 0) // if pulse finished TA1CCR0 = [ result - TA1CCR0] put in heating mode
         {
           TA1CCTL1 = OUTMOD_7; //PWM reset/set output mode
           TA1CCR0 = result - TA1CCR0;  
           P2OUT |= BIT0; // H-bridge to heating mode
           break;
         }
       }
     }
     else  // if in heating mode 
     {
     while(1)
       {
         if(TA0R == 0) // if pulse finished add result
         {
           TA1CCR0 += result;
           break;
         }
       }
     }
    }
    else// if the result of result and TA1CCR0 dictates that the PWM is/should be in cooling mode and the PMW is not to have a duty cycle less then 0% if in cooling mode
    {
     if(P2OUT_bit.P0 == 1) // if in heating mode 
     {
       while(1)
       {
         if(TA0R == 0) // if pulse finished TA1CCR0 = [(- result) - TA1CCR0] put in cooling mode
         {
           TA1CCTL1 = OUTMOD_3; //PWM set/reset output mode
           TA1CCR0 =  -1 * result - TA1CCR0;   
           P2OUT &= ~BIT0; // H-bridge to cooling mode
           break;
         }
       }
     
     }
     else // if in cooling mode
     {
     while(1)
       {
         if(TA0R == 0)// if pulse finished add (- result)
         {
           TA1CCR0 -= result;
           break;
         }
       }
      }
    }
  }
}
 
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
   
     UCA0TXBUF = samples[++countSend]; // TX next character 
     if (countSend == sizeof samples -1)
     { 
       UC0IE &= ~UCA0TXIE; // Disable USCI_A0 TX interrupt 
     }
} 

#pragma vector=USCIAB0RX_VECTOR 
__interrupt void USCI0RX_ISR(void) 
{  
           countSend = 0;    // reset the count sent
           UC0IE |= UCA0TXIE; // interrupt TXD enabled
           UCA0TXBUF = samples[countSend]; // send out sample trigger interrupt TXD
           
           setTemp = (float)(UCA0RXBUF);  // receive at temperature to set
           while (!IFG2_bit.UCA0RXIFG);
           setTemp += ((float)(UCA0RXBUF))/10;
           
           //cap the set temperature from [0 40]
           if(setTemp < 0)
           {
             setTemp = 0;
           }
           else if(setTemp > 40)
           {
             setTemp = 40;
           }
}

void init()
{
 
   WDTCTL = WDTPW + WDTHOLD; // Stop WDT
   
   BCSCTL1 = CALBC1_16MHZ; // Set high-speed digitally controlled oscillator clock 16Mhz
   DCOCTL = CALDCO_16MHZ;
   
   P2DIR = 0xFF; // All P2 pins outputs
   P2OUT &= 0x00; // All P2.x V-low
   P1OUT &= 0x00;  //All P1.x V-low
   P1SEL |= BIT1 + BIT2; // P1.1 = RXD, P1.2=TXD
   P1SEL2 |= BIT1 + BIT2; 
   
    UCA0CTL1 |= UCSSEL_2; // SMCLK
   UCA0BR0 = 0x08; // 16MHz 115200 baud
   UCA0BR1 = 0x00; // 16MHz 115200 baud
   
    UCA0MCTL = UCBRF_11; // 16MHz 115200 baud
    UCA0MCTL |= UCOS16;  //Oversampling mode enabled
    UCA0CTL1 &= ~UCSWRST; // Initialize USCI state machine
   
    UC0IE |= UCA0RXIE; // Enable USCI_A0 RX interrupt
   // Enter LPM0 w/ int until Byte RXed
    
   // initialize ADC Module
// select P1.4 as ADC input
// select continuous mode

    // use Mode 2 - Repeat single channel
    ADC10CTL1 = INCH_4 + CONSEQ_2; // use P1.4 (channel 4)
    ADC10AE0 |= BIT4; // enable analog input channel 4
    //select sample-hold time, multisample conversion and turn on the ADC
    ADC10CTL0 |= ADC10SHT_0 + MSC + ADC10ON;
    // start ADC
    ADC10CTL0 |= ADC10SC + ENC;
    
    //PWMc set up
      P2SEL2_bit.P1 = 0; // PWM output p2.1
      P2SEL_bit.P1 = 1;
  
  TA1CTL = TASSEL_2 + ID_0 + MC_2; // clock source SMCLK + div /2 +  continous count up
  
  TA1CCTL1 = OUTMOD_3; // PWM set/reset output mode
  TA1CCR1 = 0;
  TA1CCR0 = 1;
 
 
    
      __enable_interrupt(); // as said
}

void main(void){
 
  
  
  init();
  while(1){ 
    
    sampler();
    calculateTemp();
    PWM_control(PIDcontroller());
   
  }
}
