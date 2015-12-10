#include "driverlib.h"

void main_init();
void uartAx_init_9600baud(uint16_t base_addr);
void init_CLK();
void init_timer();


#define start_transmit 0x55

#define COMPARE_VALUE 3276

volatile bool next = 0;
volatile bool begin_tx = 0;
volatile bool resend_byte = 0;

int main(void) {

	uint16_t i = 0;

    WDT_A_hold(WDT_A_BASE);

    uint8_t laser_data = 0x55;

    P4DIR |= 0x08;
    P4OUT |= 0x08;

    main_init();
    init_timer();



    for(;;){
    	if(begin_tx){
    		begin_tx = 0;
    		init_timer();
    		Timer_A_startCounter(TIMER_A1_BASE,
    					TIMER_A_CONTINUOUS_MODE);
    		P4OUT |= 0x08;
    		while(!next){}
    		next = 0;
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
    		if(resend_byte){
    			//insert resend byte logic here
    			resend_byte = 0;
    		}
    	}

    }





    return (0);
}

void main_init(){
	init_CLK();
	uartAx_init_9600baud(__MSP430_BASEADDRESS_EUSCI_A1__);
    UCA1IE |= 0x01;		//enable RX inturrupt
	uartAx_init_9600baud(__MSP430_BASEADDRESS_EUSCI_A0__);
	PM5CTL0 &= ~LOCKLPM5;	//Unlock Pins
	__bis_SR_register(GIE);
}

//Sources MCLK and SMCLK from the DCO which is set to 16MHz
//Sources ACLK from LFXT
void init_CLK(){
	FRCTL0 = FRCTLPW | NWAITS_1;						 // Configure one FRAM waitstate as required by the device datasheet for MCLK
	  	  	  	  	  	  	  	  	  	  	  	  	  	 // operation beyond 8MHz _before_ configuring the clock system.
	CSCTL0_H = CSKEY >> 8;                               // Unlock CS registers
	CSCTL1 = DCORSEL | DCOFSEL_4;                        // Set DCO to 16MHz
	CSCTL2 = SELS__DCOCLK | SELM__DCOCLK; 				 // Set SMCLK = MCLK = DCO
	CSCTL3 = DIVS__1 | DIVM__1;                			 // Set all dividers
	CSCTL0_H = 0;                                        // Lock CS registers

	PJSEL0 |= 0x30;										 //Set pins J.4 and J.5 for LFXIN and LFXOUT respectivly
	PJSEL1 &= ~0x30;									 //This is neccesary for LFXT to work

	CS_setExternalClockSource(32768,					//set LFXT to 32768 KHz and HFXT to 4 MHz
	                          4000000);
	CS_turnOnLFXT(CS_LFXT_DRIVE_3);						//enable LFXT for operation with a 32768 KHz crystal
	CS_initClockSignal(CS_ACLK,							//set ACLK to source from the LFXT undivided
					   CS_LFXTCLK_SELECT,
				       CS_CLOCK_DIVIDER_1);
}

//	Inputs: base_addr - this is the base address of the interface one aims to initialize
//	Purpose:This function will initialize the desired eUSCI interface to function
//			as a UART sourced from a 16 MHz SMCLK with a baud rate of 9600.
//	Notes:	Visit the link below to determine clockPrescalar, firstModReg, and secondModReg values for
//			a given CLK source frequency and target baud rate
//			http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
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
	uart_param.clockPrescalar           = 104;
	uart_param.firstModReg 			 	= 2;  //these values taken from TIs calculator tool, 5739 defaults to 16 MHz
	uart_param.secondModReg             = 182;
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
//Timer1_A3
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

#pragma vector = USCI_A1_VECTOR
__interrupt void UARTA1_ISR(void){
    switch(__even_in_range(UCA1IV,16)){
        case 0:	break;				//No interrupt flags
        case 2:
        	if(UCA1RXBUF == start_transmit){  ///TODO insert parsing logic here
        		begin_tx = 1;
        	}
        	else if(UCA1RXBUF == failed_byte){
        		resend_byte = 1;
        	}
        	break;				// UCRXIFG: Triggered when data moves from the RX register into the RX buffer; repersents end of transmission
        case 4: 					//UCTXIFG: Triggered when data moves form TX buffer into TX register
        	break;
        case 6:	break;				//UCSTTIFG: Start bit received
		case 8: break;				//UCTXCPTIFG: Transmit complete
        default: break;
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
