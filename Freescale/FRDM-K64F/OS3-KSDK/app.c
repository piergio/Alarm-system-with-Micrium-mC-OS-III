/*
*********************************************************************************************************
*
*                                        Micrium uC/OS-III and
*                                        Freescale Kinetis K64
*                                               on the
*
*                                         Freescale FRDM-K64F
*                                          Evaluation Board
*
*
* Set up an interrupt handler that turns on/off the blue led when SW1 is toggled
* Must set ".config.interrupt = kPortIntFallingEdge" for switch definition in "gpio_pins.c"
*********************************************************************************************************
*/

#include "fsl_interrupt_manager.h"

#include  <math.h>
#include  <lib_math.h>
#include  <cpu_core.h>

#include  <app_cfg.h>
#include  <os.h>

#include  <fsl_os_abstraction.h>
#include  <system_MK64F12.h>
#include  <board.h>

#include  <bsp_ser.h>

#include <fsl_gpio_common.h>    // externs g_PortBaseAddr needed in ISR

//#define PERIODIC_TIMER

#define GREEN_LED	1
#define RED_LED		2
#define BLUE_LED	3

#define FIXED		4
#define F10HZ		5
#define F20HZ		6

						/********************	Global variables	********************/

volatile	uint16_t	val = 0;
volatile	int			led = 0;
volatile	int			freq = 0;

static		OS_SEM		sem1;
static		OS_SEM		sem2;

static		OS_TCB		StartTaskTCB;		// Task Control Block for maintaining information of StartTask in memory

#ifndef PERIODIC_TIMER
static		OS_TCB		LEDTaskTCB;			// Task Control Block for maintaining information of LEDTask in memory
#endif

static		OS_TCB		ADCTaskTCB;			// Task Control Block for maintaining information of ADCTask in memory


static		CPU_STK		StartTaskStack[APP_CFG_TASK_START_STK_SIZE];		// Stack space for StartTask

#ifndef PERIODIC_TIMER
static		CPU_STK		LEDTaskStack[APP_CFG_TASK_START_STK_SIZE];			// Stack space for LEDTask
#endif

static		CPU_STK		ADCTaskStack[APP_CFG_TASK_START_STK_SIZE];			// Stack space for ADCTask


						/********************	Prototype functions		********************/

		/* Setup functions*/
static void setupUART();
static void setupADC0();
static uint16_t readADC0();

		/* Interrupt Handlers */
void ADC_InterruptHandler();

#ifdef PERIODIC_TIMER
	void LED_InterruptHandler();
#endif

		/* Task functions */
#ifndef PERIODIC_TIMER
	static void LEDTask();
#endif

static void ADCTask();
static void startTask();

		/* Work functions */
#ifdef PERIODIC_TIMER
	void getFLED();
#endif


						/********************	Main function	********************/
int main (void){

    OS_ERR err;

#if (CPU_CFG_NAME_EN == DEF_ENABLED)
    CPU_ERR  cpu_err;
#endif

    hardware_init();												// Enables clock for PORT
    GPIO_DRV_Init(switchPins, ledPins);								// Initializes GPIO pins (input, output)
    setupADC0();													// Sets ADC0 module

#if (CPU_CFG_NAME_EN == DEF_ENABLED)
    CPU_NameSet((CPU_CHAR *)"MK64FN1M0VMD12", (CPU_ERR  *)&cpu_err);
#endif

    /* Managing Interrupt for ADC0 */
    INT_SYS_EnableIRQ(ADC0_IRQn);									// Enables the interrupt signals on ADC0_IRQn
    INT_SYS_InstallHandler(ADC0_IRQn,	ADC_InterruptHandler);		// Associates ISR with the interrupt ADC0 source

#ifdef PERIODIC_TIMER
    /* Managing PIT0 module */
    INT_SYS_EnableIRQ(PIT0_IRQn);									// Enables the interrupt signals on PIT0_IRQn
	INT_SYS_InstallHandler(PIT0_IRQn, LED_InterruptHandler);		// Associates ISR with the interrupt PIT0 source

	SIM_SCGC6 |= SIM_SCGC6_PIT_MASK;								// Enables clock gate on PIT0 module
	PIT_TCTRL0 |= PIT_TCTRL_TIE_MASK | PIT_TCTRL_TEN_MASK;			// Enables timer interrupt into the control register
#endif

	OSA_Init();                                                 	// Initializes uC/OS-III

    /* Creates the START task and inserts it into the Ready List according to its priority level */
	OSTaskCreate(&StartTaskTCB,
						 "Start Task",
						 startTask,
						 0u,
						 APP_CFG_TASK_START_PRIO,
						 &StartTaskStack[0u],
						 (APP_CFG_TASK_START_STK_SIZE / 10u),
						 APP_CFG_TASK_START_STK_SIZE,
						 0u,
						 0u,
						 0u,
						 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
						 &err);

    OSA_Start();        	// Starts multitasking

    while (DEF_ON) {
     ;						// Nothing to do
    }
}

static void startTask(){

	OS_ERR task_err;
	OS_ERR sem_err;
	OS_ERR del_err;

	CPU_Init();													// Initializes CPU module

	OSSemCreate(&sem1, "MySem1", (OS_SEM_CTR)0, &sem_err);		// Creates a semaphore

#ifdef PERIODIC_TIMER
	OSSemCreate(&sem2, "MySem2", (OS_SEM_CTR)0, &sem_err);		// Creates a semaphore
#endif

	/* Creates the ADC task */
	OSTaskCreate(&ADCTaskTCB,
						 "ADC Task",
						 ADCTask,
						 0u,
						 APP_CFG_TASK_START_PRIO,
						 &ADCTaskStack[0u],
						 (APP_CFG_TASK_START_STK_SIZE / 10u),
						 APP_CFG_TASK_START_STK_SIZE,
						 0u,
						 0u,
						 0u,
						 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
						 &task_err);

#ifndef PERIODIC_TIMER
	/* Creates the LED task */
	OSTaskCreate(&LEDTaskTCB,
					 "LED Task",
					 LEDTask,
					 0u,
					 APP_CFG_TASK_START_PRIO,
					 &LEDTaskStack[0u],
					 (APP_CFG_TASK_START_STK_SIZE / 10u),
					 APP_CFG_TASK_START_STK_SIZE,
					 0u,
					 0u,
					 0u,
					 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP),
					 &task_err);

#endif

#ifdef PERIODIC_TIMER
	while(DEF_TRUE){
		getFLED();							// Reads AD converted value from a global variable and sets period
	}
#endif

#ifndef PERIODIC_TIMER
	OSTaskDel((OS_TCB *)0, &del_err);		// Deletes starter task
#endif

}

static void setupADC0(){

	SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;		// Enables clock gate on ADC0 module
	ADC0_CFG1 |= ADC_CFG1_MODE(3);			// 16bits ADC
	ADC0_SC1A |= ADC_SC1_ADCH(31);			// Disables the module, ADCH = 1111
}

static uint16_t readADC0(){

	ADC0_SC1A = 0x4C; 						// Writes to SC1A to start conversion and enable generating interrupts when conversion is complete
}

static void ADCTask(){

	OS_ERR sem_err;

	/* ADC reading */
	while(DEF_TRUE){

		readADC0();														// Calls to Read ADC0
		OSSemPend(&sem1, 0u, OS_OPT_PEND_BLOCKING, 0u, &sem_err);		// Waits on semaphore sem1 if it is busy
	}
}

void ADC_InterruptHandler(){

	OS_ERR sem1_err;
	OS_ERR sem2_err;

	CPU_CRITICAL_ENTER();	// Enters critical section (disable interrupts)

	OSIntEnter();         	// Notifies to scheduler the beginning of an ISR

	val = ADC0_RA;			// Stores converted value to a variable

	OSSemPost(&sem1, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED, &sem1_err);		// Releases semaphore sem1

#ifdef PERIODIC_TIMER
	OSSemPost(&sem2, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED, &sem2_err);		// Releases semaphore sem2
#endif

	CPU_CRITICAL_EXIT();  	// Enable interrupts

	OSIntExit();			// Notifies to scheduler the end of an ISR
}

#ifdef PERIODIC_TIMER
void getFLED(){

	OS_ERR sem_err;

	OSSemPend(&sem2, 0u, OS_OPT_PEND_BLOCKING, 0u, &sem_err);		// Waits on semaphore sem2 if it is busy

	if(val<9929){

			// (0-0.5V) - Blink green led at 10Hz
			PIT_MCR = 0x02;				// Disables PIT module
			PIT_LDVAL0 = 0x2DC6C0;		// Sets time period
			PIT_MCR = 0x0;				// Enables PIT module
			led = GREEN_LED;			// Sets LED for the next blinking
			freq = F10HZ;				// Sets also the Frequency

		}else if(val>=19859 && val<29789){

			// (1-1.5V) - Blink blue led at 10Hz
			PIT_MCR = 0x02;
			PIT_LDVAL0 = 0x2DC6C0;
			PIT_MCR = 0x0;
			led = BLUE_LED;
			freq = F10HZ;

		}else if(val>=39718 && val<49648){

			// (2-2.5V) - Blink red led at 10Hz
			PIT_MCR = 0x02;
			PIT_LDVAL0 = 0x2DC6C0;
			PIT_MCR = 0x0;
			led = RED_LED;
			freq = F10HZ;

		}else if(val>=9929 && val<19859){

			// (0.5-1V) - Blink green led at 20Hz
			PIT_MCR = 0x02;
			PIT_LDVAL0 = 0x16E360;
			PIT_MCR = 0x0;
			led = GREEN_LED;
			freq = F20HZ;

		}else if(val>=29789 && val<39718){

			// (1.5-2V) - Blink blue led at 20Hz
			PIT_MCR = 0x02;
			PIT_LDVAL0 = 0x16E360;
			PIT_MCR = 0x0;
			led = BLUE_LED;
			freq = F20HZ;

		}else if(val>=49648 && val<59578){

			// (2.5-3.3V) - Blink red led at 20Hz
			PIT_MCR = 0x02;
			PIT_LDVAL0 = 0x16E360;
			PIT_MCR = 0x0;
			led = RED_LED;
			freq = F20HZ;

		}else{

			// Otherwise turn on red LED
			led = RED_LED;
			freq = FIXED;
		}
}

void LED_InterruptHandler(){

	CPU_CRITICAL_ENTER();	// Enters critical section (disable interrupts)

	OSIntEnter();         	// Notifies to scheduler the beginning of an ISR

	PIT_TFLG0 = PIT_TFLG_TIF_MASK;											// Clears Timer Interrupt Flag

	if(led == GREEN_LED){

		// (0-0.5V) - Blink green led
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);						// turns off red LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
		GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_GREEN);						// turns on/off green led

	}else if(led == BLUE_LED){

		// (1-1.5V) - Blink blue led
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);						// turns off red LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off blue LED
		GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_BLUE);						// turns on/off blue led

	}else if(led == RED_LED && freq != FIXED){

		// (2-2.5V) - Blink red led
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off green LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
		GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_RED);						// turns on/off red led

	}else if(led == RED_LED && freq == FIXED){

		// Otherwise turn on fixed red LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turn off green LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turn off blue LED
		GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 0);						// turn on red LED
	}

	CPU_CRITICAL_EXIT();  	// Enable interrupts

	OSIntExit();			// Notifies to scheduler the end of an ISR

}
#endif

#ifndef PERIODIC_TIMER
static void LEDTask(){

	// 0.5V ->	9929
	// 1V 	->	19859
	// 1.5V ->	29789
	// 2V 	->	39718
	// 2.5V ->	49648
	// 3V 	->	59578

	OS_ERR os_err;
	OS_ERR sem_err;

	while(DEF_TRUE){

		if(val<9929){

			// (0-0.5V) - Blink green led at 10Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);							// turns off red LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);						// turns off blue LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_GREEN);							// turns on/off green led
			OSTimeDlyHMSM(0u, 0u, 0u, 50u, OS_OPT_TIME_HMSM_STRICT, &os_err);		// delays 50ms

		}else if(val>=19859 && val<29789){

			// (1-1.5V) - Blink blue led at 10Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);						// turns off red LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off blue LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_BLUE);						// turns on/off blue led
			OSTimeDlyHMSM(0u, 0u, 0u, 50u, OS_OPT_TIME_HMSM_STRICT, &os_err);	// delay 50ms

		}else if(val>=39718 && val<49648){

			// (2-2.5V) - Blink red led at 10Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off green LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_RED);						// turns on/off red led
			OSTimeDlyHMSM(0u, 0u, 0u, 50u, OS_OPT_TIME_HMSM_STRICT, &os_err);	// delays 50ms

		}else if(val>=9929 && val<19859){

			// (0.5-1V) - Blink green led at 20Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);						// turns off red LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_GREEN);						// turns on/off green led
			OSTimeDlyHMSM(0u, 0u, 0u, 25u, OS_OPT_TIME_HMSM_STRICT, &os_err);	// delays 25ms

		}else if(val>=29789 && val<39718){

			// (1.5-2V) - Blink blue led at 20Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off green LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 1);						// turns off red LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_BLUE);						// turns on/off blue led
			OSTimeDlyHMSM(0u, 0u, 0u, 25u, OS_OPT_TIME_HMSM_STRICT, &os_err);	// delays 25ms

		}else if(val>=49648 && val<59578){

			// (2.5-3.3V) - Blink red led at 20Hz
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off green LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
			GPIO_DRV_TogglePinOutput(BOARD_GPIO_LED_RED);						// turns on/off red led
			OSTimeDlyHMSM(0u, 0u, 0u, 25u, OS_OPT_TIME_HMSM_STRICT, &os_err);	// delays 25ms

		}else{

			// Otherwise turn on red LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_GREEN, 1);					// turns off green LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_BLUE, 1);					// turns off blue LED
			GPIO_DRV_WritePinOutput(BOARD_GPIO_LED_RED, 0);						// turns on red fixed LED
		}

	}
}
#endif
