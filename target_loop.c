#include "driverlib.h"
#include <cstdio>

#define COMPARE_VALUE 3276

void uartAx_init_9600baud(uint16_t base_addr);
void init_ACLK();
void init_timer();
void calibrate();
uint8_t adc_sweep();

volatile uint16_t ambient_av = 0;

volatile uint8_t next = 0;

int main(void) {

	WDT_A_hold(WDT_A_BASE);

	uint16_t signal_lock = 0;
	uint16_t ones = 0;
	uint16_t zeros = 0;

	uint16_t a = 0;
	uint16_t i = 0;

	uint16_t av= 0;

	uint16_t connect = 0;

	uint16_t results[9];

	uint16_t laser_data;

	uint8_t init_success = 0;

	// Enable switches
	// P4.0 and P4.1 are configured as switches
	// Port 4 has only two pins /S1 == P4.0 S2 == P4.1
	P4OUT |= BIT0 +BIT1;                      // Configure pullup resistor
	P4DIR &= ~(BIT0 + BIT1);                  // Direction = input
	P4REN |= BIT1;                     // Enable pullup resistor
	P4IES |= BIT0 + BIT1;                    // P4.0 Lo/Hi edge interrupt
	P4IE = (BIT1);                         // P4.0 interrupt enabled
	P4IFG = 0;                                // P4 IFG cleared

	P1DIR |= 0x80;
	P4DIR |= 0x01;

	P1OUT &= ~0x80;
	P4OUT &= ~0x01;

	P1SEL1 |= 0x3F;
	P1SEL0 |= 0x3F;

	P3SEL1 |= 0x0F;
	P3SEL0 |= 0x0F;

	uartAx_init_9600baud(__MSP430_BASEADDRESS_EUSCI_A1__);
	init_ACLK();
	init_timer();

	for(;;){
		init_success = ADC10_B_init(ADC10_B_BASE,
				ADC10_B_SAMPLEHOLDSOURCE_SC,
				ADC10_B_CLOCKSOURCE_ADC10OSC,
				ADC10_B_CLOCKDIVIDER_1);

		if(init_success == STATUS_SUCCESS){
			break;
		}
	}

	//high-to-low SAMPCON transition starts AD conversion, AD conversion
	//takes 11 ADC10CLK cycles

	ADC10_B_enable(ADC10_B_BASE);

	ADC10_B_setupSamplingTimer(ADC10_B_BASE,
			ADC10_B_CYCLEHOLD_16_CYCLES,
			ADC10_B_MULTIPLESAMPLESENABLE);

	ADC10_B_configureMemory(ADC10_B_BASE,
			ADC10_B_INPUT_A14,
			ADC10_B_VREFPOS_AVCC,
			ADC10_B_VREFNEG_AVSS);

	//this for loop samples ADCs 0-5 and 12-15

	//A0 - p1.0  [0]
				  //A1 - p1.1  [1]
	//A2 - p1.2  [2]
	//A3 - p1.3  [3]
	//A4 - p1.4  [4]
	//A5 - p1.5  [5]
	//A12 - p3.0 [6]
	//A13 - p3.1 [7]
	//A14 - p3.2 [8]
	calibrate();
	P4IFG = 0;
	__bis_SR_register(GIE);
	for(;;){
		__bic_SR_register(GIE);
		for(i = 0; i < 9; ++i){
			if(i < 6){
				ADC10CTL0 &= ~0x02; 		//unlock regs
				ADC10MCTL0 &= ~0x0F;		//clear input
				ADC10MCTL0 |= i;
				ADC10CTL0 |= 0x02;			//lock regs
			}
			else{
				ADC10CTL0 &= ~0x02; 		//unlock regs
				ADC10MCTL0 &= ~0x0F;		//clear input
				ADC10MCTL0 |= (i + 6);
				ADC10CTL0 |= 0x02;			//lock regs
			}
			ADC10_B_startConversion(ADC10_B_BASE,
					ADC10_B_SINGLECHANNEL);
			for(;;){
				if(ADC10IFG & 0x01){
					results[i] = ADC10MEM0;
					break;
				}
			}
		}

		av = 0;
		for(i = 0; i < 9; ++i){
			av += results[i];
		}
		av = av/9;
		__bis_SR_register(GIE);
		a = a + 1;  //for debugging purposes!!

		if(av < (ambient_av - 3)){
			P1OUT &= ~0x80;
			P4OUT |= 0x01;
			connect += 1;
		}
		else{
			P1OUT |= 0x80;
			P4OUT &= ~0x01;
			connect = 0;
		}

		if(connect > 200){															//look for 200 cycles in a row of lser on array
			EUSCI_A_UART_transmitData(__MSP430_BASEADDRESS_EUSCI_A1__,				//send start signal to base
					0x55);
			while(UCA1STATW & 0x01){}												//wait for UART bus to clear
			//~8.1ms between end of UART TX to RX on base
			__delay_cycles(129600);  												//Wait 8.1ms for XBEE latency
			init_timer();															//setup timer
			Timer_A_startCounter(TIMER_A1_BASE,										//start timer
					TIMER_A_CONTINUOUS_MODE);
			ones = 0;
			zeros = 0;
			while(!next){															//wait for start frame timer
				if(adc_sweep()){													//run array sweep
					ones++;															//increment 1s if avg below thresh
				}
				else{
					zeros++;														//increment 0s if avg above thresh
				}
			}
			if(ones >= zeros){														//check if start bit transferred successfully
				signal_lock = 1;													//if success set signal lock to cont.
			}
			else{
				signal_lock = 0;													//else set signal_lock lost and inform base
				EUSCI_A_UART_transmitData(__MSP430_BASEADDRESS_EUSCI_A1__,				//send start signal to base
									0x66);
			}
			next = 0;
			if(signal_lock){														//if start sent success continue read
				for(i = 0; i < 8; i++){
					if(laser_data & (1<<i)){
						P4OUT |= 0x08;
					}
					else{
						P4OUT &= ~0x08;
					}
					while(!next){}
					next = 0;
				}
				P4OUT |= 0x08;
				while(!next){}
				Timer_A_stop(TIMER_A1_BASE);
				next = 0;
			}
		}
		connect = 0;
	}




}

uint8_t read_bit(){

}

uint8_t adc_sweep(){
	uint16_t i = 0;
	uint16_t avg = 0;
	uint16_t results[9];

	__bic_SR_register(GIE);
	    	for(i = 0; i < 9; ++i){
	    		if(i < 6){
	    			ADC10CTL0 &= ~0x02; 		//unlock regs
	    			ADC10MCTL0 &= ~0x0F;		//clear input
	    			ADC10MCTL0 |= i;
	    			ADC10CTL0 |= 0x02;			//lock regs
	    		}
	    		else{
	    			ADC10CTL0 &= ~0x02; 		//unlock regs
	    			ADC10MCTL0 &= ~0x0F;		//clear input
	    			ADC10MCTL0 |= (i + 6);
	    			ADC10CTL0 |= 0x02;			//lock regs
	    		}
	    		ADC10_B_startConversion(ADC10_B_BASE,
	    	    					ADC10_B_SINGLECHANNEL);
	    		for(;;){
	    			if(ADC10IFG & 0x01){
	    				results[i] = ADC10MEM0;
	    				break;
	    			}
	    		}
	    	}

	    	avg = 0;
	    	for(i = 0; i < 9; ++i){
	    		avg += results[i];
	    	}
	    	avg = avg/9;
	    	if(avg < (ambient_av - 3)){
	    		P1OUT &= ~0x80;
	    		P4OUT |= 0x01;
	    		return 1;
	    	}
	    	else{
	    		P1OUT |= 0x80;
	    		P4OUT &= ~0x01;
	    		return 0;
	    	}

	    	__bis_SR_register(GIE);
}

void uartAx_init_9600baud(uint16_t base_addr){
	if(base_addr == 0x05E0){
		GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2,
				  	  	        				   GPIO_PIN5 + GPIO_PIN6,
												   GPIO_SECONDARY_MODULE_FUNCTION
				  	  	        				   );
	}
	else{
		GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2,
						  	  	        		   GPIO_PIN0 + GPIO_PIN1,
												   GPIO_SECONDARY_MODULE_FUNCTION
						  	  	        		   );
	}

	EUSCI_A_UART_disable(base_addr);


	EUSCI_A_UART_initParam uart_param;
	uart_param.selectClockSource        = EUSCI_A_UART_CLOCKSOURCE_SMCLK;
	uart_param.clockPrescalar           = 6;
	uart_param.firstModReg 			 	= 8;  //these values taken from TIs calculator tool
	uart_param.secondModReg             = 17;
	uart_param.parity                   = EUSCI_A_UART_NO_PARITY;
	uart_param.msborLsbFirst            = EUSCI_A_UART_LSB_FIRST;
    uart_param.numberofStopBits         = EUSCI_A_UART_ONE_STOP_BIT;
    uart_param.uartMode                 = EUSCI_A_UART_MODE;
	uart_param.overSampling			 	= EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION;

    EUSCI_A_UART_init(base_addr,
		              &uart_param);

    EUSCI_A_UART_enable(base_addr);
    //INTURRUPTS MUST BE ENABLED AFTER EUSCI_A_UART_enable() IS CALLED
}

void init_ACLK(){

	PJSEL0 |= 0x30;										 //Set pins J.4 and J.5 for LFXIN and LFXOUT respectivly
	PJSEL1 &= ~0x30;									 //This is neccesary for LFXT to work

	CS_setExternalClockSource(32768,					//set LFXT to 32768 KHz and HFXT to 4 MHz
	                          4000000);
	CS_turnOnXT1(CS_XT1_DRIVE_3);						//enable LFXT for operation with a 32768 KHz crystal
	CS_initClockSignal(CS_ACLK,							//set ACLK to source from the LFXT undivided
					   CS_XT1CLK_SELECT,
				       CS_CLOCK_DIVIDER_1);
}

void calibrate(){
	uint16_t sults[9];
	uint32_t averg = 0;
	uint16_t avergs[100];

	uint16_t k = 0;
	uint16_t j = 0;

	for(j = 0; j < 100; ++j){

		for(k = 0; k < 9; ++k){
			if(k < 6){
				ADC10CTL0 &= ~0x02; 		//unlock regs
				ADC10MCTL0 &= ~0x0F;		//clear input
				ADC10MCTL0 |= k;
				ADC10CTL0 |= 0x02;			//lock regs
			}
			else{
				ADC10CTL0 &= ~0x02; 		//unlock regs
				ADC10MCTL0 &= ~0x0F;		//clear input
				ADC10MCTL0 |= (k + 6);
				ADC10CTL0 |= 0x02;			//lock regs
			}
			ADC10_B_startConversion(ADC10_B_BASE,
					ADC10_B_SINGLECHANNEL);
			for(;;){
				if(ADC10IFG & 0x01){
					sults[k] = ADC10MEM0;
					break;
				}
			}
		}

		averg = 0;
		for(k = 0; k < 9; ++k){
			averg += sults[k];
		}
		avergs[j] = averg/9;
	}
	averg = 0;
	for(j = 0; j < 100; ++j){
		averg += avergs[j];
	}
	ambient_av = averg/100;
}

void init_timer(){
	Timer_A_initContinuousModeParam initContParam = {0};
	initContParam.clockSource = TIMER_A_CLOCKSOURCE_ACLK;
	initContParam.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
	initContParam.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;
	initContParam.timerClear = TIMER_A_DO_CLEAR;
	initContParam.startTimer = false;
	Timer_A_initContinuousMode(TIMER_A1_BASE, &initContParam);

	//Initiaze compare mode
	Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,
			TIMER_A_CAPTURECOMPARE_REGISTER_0
	);

	Timer_A_initCompareModeParam initCompParam = {0};
	initCompParam.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_0;
	initCompParam.compareInterruptEnable =
			TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE;
	initCompParam.compareOutputMode = TIMER_A_OUTPUTMODE_OUTBITVALUE;
	initCompParam.compareValue = COMPARE_VALUE;
	Timer_A_initCompareMode(TIMER_A1_BASE, &initCompParam);
}

#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void)
{
  switch(__even_in_range(P4IV,P4IV_P4IFG1))
    {
      case P4IV_P4IFG0:
    	  calibrate();
      case P4IV_P4IFG1:
    	  calibrate();
      default:
        break;
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER1_A0_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(TIMER1_A0_VECTOR)))
#endif
void TIMER1_A0_ISR(void)
{
    uint16_t compVal = Timer_A_getCaptureCompareCount(TIMER_A1_BASE,
                                                      TIMER_A_CAPTURECOMPARE_REGISTER_0)
                       + COMPARE_VALUE;

    //Add Offset to CCR0
    Timer_A_setCompareValue(TIMER_A1_BASE,
                            TIMER_A_CAPTURECOMPARE_REGISTER_0,
                            compVal
                            );

    next = 1;
}
