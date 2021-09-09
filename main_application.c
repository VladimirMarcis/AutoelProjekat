/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH (0)
#define COM_CH1 (1)

	/* TASK PRIORITIES */
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAl_REC_PRI			( tskIDLE_PRIORITY + 3 )
#define	SERVICE_TASK_PRI		( tskIDLE_PRIORITY + 1 )

/* TASKS: FORWARD DECLARATIONS */
static void Serial_receive_tsk_vrata(void* pvParameters);
static void Senzori_inf(void* pvParameters);
static uint32_t processRXCInterrupt(void);
static void Serial_send_senzori(TimerHandle_t timer1);
static void Serial_receive_tsk_PC(void* pvParameters);
static void Serial_send_PC(void* pvParameters);
static void Led_bar_tsk(void* pvParameters);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
static const char trigger[] = "Z";
static uint8_t volatile t_point;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
static uint8_t r_buffer[R_BUF_SIZE];
static uint8_t volatile r_point;


/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const uint8_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

static SemaphoreHandle_t RXC_BinarySemaphore0, RXC_BinarySemaphore1, LED_int_BinarySemaphore;
static BaseType_t status;
static TimerHandle_t timer1, timer2;
static QueueHandle_t Queue_vrata, Queue_brzina, Queue_senzori, Queue_PC, Queue_prekidac, Queue_serijska, Queue_otvorena_vrata;
static QueueHandle_t Queue_gepek;

typedef struct _Senzori_struct {
	uint8_t vrata[10];
	uint8_t brzina;
} Senzori_struct;


static void Led_bar_tsk(void* pvParameters) {

	uint8_t prekidac = 0;
	uint8_t serijska = 0;
	uint8_t otvorena_vrata = 0;
	uint8_t blink = 0xff;
	uint8_t gepek = 0;

	for (;;) {

		if (xSemaphoreTake(LED_int_BinarySemaphore, pdMS_TO_TICKS(200)) != pdTRUE)
		{
			//printf("Greska prilikom preuzimanja semafora2 \n");
		}

		if (xQueueReceive(Queue_serijska, &serijska, pdMS_TO_TICKS(200)) != pdTRUE)
		{
			//printf("Preuzimanje podataka iz reda6 nije uspelo");
		}

		if (xQueueReceive(Queue_otvorena_vrata, &otvorena_vrata, portMAX_DELAY) != pdTRUE)
		{
			printf("Preuzimanje podataka iz reda6 nije uspelo");
		}

		if (xQueueReceive(Queue_gepek, &gepek, portMAX_DELAY) != pdTRUE) {
			printf("Preuzimanje podataka iz reda6 nije uspelo");
		}

		if (get_LED_BAR(0, &prekidac) != 0)
		{
			printf("Greska prilikom ocitavanja");
		}

		if ((gepek == (uint8_t)0x31) && (prekidac == (uint8_t)0x01) && (serijska == (uint8_t)1))
		{
			if (set_LED_BAR(1, 0x01) != 0)
			{
				printf("Greska prilikom setovanja LED bara");
			}
		}

		if ((gepek == (uint8_t)0x30) || (prekidac == (uint8_t)0x00) || (serijska == (uint8_t)0))
		{
			if (set_LED_BAR(1, 0x00) != 0)
			{
				printf("Greska prilikom setovanja LED bara");
			}
		}

		if (otvorena_vrata == (uint8_t)1)
		{
			if (set_LED_BAR(2, blink) != 0)
			{
				printf("Greska prilikom setovanja LED bara");
			}
			vTaskDelay(pdMS_TO_TICKS(500));
			if (set_LED_BAR(2, ~blink) != 0)
			{
				printf("Greska prilikom setovanja LED bara");
			}
		}

		if (xQueueReset(Queue_prekidac) != pdTRUE)
		{
			printf("Greska prilikom resetovanja reda");
		}

		if (xQueueSend(Queue_prekidac, &prekidac, 0) != pdTRUE)
		{
			printf("Slanje podataka u red5 nije uspelo");
		}
	}
}


static void Serial_send_PC(void* pvParameters) {

	Senzori_struct senzori;
	uint8_t temp = 0;
	uint8_t vrata = 0;
	uint8_t poruka = 0;
	uint8_t prekidac = 0;
	uint8_t serijska = 0;
	static uint8_t otvorena_vrata = 0;

	for (;;) {

		if (xQueueReceive(Queue_senzori, &senzori, portMAX_DELAY) != pdTRUE)
		{
			printf("Preuzimanje podataka iz reda3 nije uspelo");
		}

		if (xQueueReceive(Queue_PC, &poruka, pdMS_TO_TICKS(100)) != pdTRUE)
		{
			//printf("Preuzimanje podataka iz reda4 nije uspelo");
			//ne moze da se ceka jako dugo jer ce da se zablokira program dok se ne posalje nesto preko serijske, zato stoji 100ms
		}

		if (xQueueReceive(Queue_prekidac, &prekidac, pdMS_TO_TICKS(100)) != pdTRUE)
		{
			printf("Preuzimanje podataka iz reda5 nije uspelo");
		}

		uint8_t stotina_brzina = (senzori.brzina) / (uint8_t)100; //npr 125 / 100 = 1
		uint8_t desetina_brzina = (((senzori.brzina) % (uint8_t)100) / (uint8_t)10); // 125 % 100 = 25, 25/10 = 2
		uint8_t jedinica_brzina = ((senzori.brzina) % (uint8_t)10); //125 % 10 = 5
		temp++;

		if ((senzori.vrata[2] == (uint8_t)0x31))
		{
			vrata = 1;
		}

		else if ((senzori.vrata[0] == (uint8_t)0x31))
		{
			vrata = 2;
		}

		else if ((senzori.vrata[3] == (uint8_t)0x31))
		{
			vrata = 3;
		}

		else if ((senzori.vrata[1] == (uint8_t)0x31))
		{
			vrata = 4;
		}

		else if ((senzori.vrata[4] == (uint8_t)0x31))
		{
			vrata = 5;
		}

		else
		{
			vrata = 0;
		}
	
		if ((senzori.brzina) > (uint8_t)5)
		{
			if ((senzori.vrata[2] == (uint8_t)0x31) || (senzori.vrata[0] == (uint8_t)0x31) || (senzori.vrata[3] == (uint8_t)0x31) || (senzori.vrata[1] == (uint8_t)0x31))
			{
				otvorena_vrata = 1;

				if (temp == (uint8_t)1)
				{
					if (send_serial_character(COM_CH1, (0x55)) != 0) //U
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)2)
				{
					if (send_serial_character(COM_CH1, (0x50)) != 0) //P
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)3)
				{
					if (send_serial_character(COM_CH1, (0x3a)) != 0) //:
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)4)
				{
					if (send_serial_character(COM_CH1, (0x20)) != 0)  //SPACE
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)5)
				{
					if (send_serial_character(COM_CH1, (0x56)) != 0) //V
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)6)
				{
					if (send_serial_character(COM_CH1, (vrata + '0')) != 0) //koja vrata su u pitanju
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)7)
				{
					if (send_serial_character(COM_CH1, (0x4f)) != 0) //0
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)8)
				{
					if (send_serial_character(COM_CH1, (0x0d)) != 0) //carriage return 
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else
				{
					temp = 0;
				}

				if ((select_7seg_digit(0)) != 0)
				{
					printf("Neuspesno selektovanje 7seg");
				}

				if ((set_7seg_digit(hexnum[vrata])) != 0)
				{
					printf("Neuspesno setovanje 7seg");
				}
			}

			else
			{
				otvorena_vrata = 0;
			}
		}

		else
		{
			otvorena_vrata = 0;
		}

		if (xQueueSend(Queue_gepek, &senzori.vrata[4], 0) != pdTRUE)
		{
			printf("Slanje podataka u red8 nije uspelo \n");
		}

		if (xQueueSend(Queue_otvorena_vrata, &otvorena_vrata, 0) != pdTRUE)
		{
			printf("Slanje podataka u red7 nije uspelo \n");
		}

		if ((senzori.vrata[4] == (uint8_t)0x31) && (prekidac == (uint8_t)1))
		{
			if ((poruka != (uint8_t)0x47))
			{
				serijska = 1;

				if (temp == (uint8_t)1)
				{
					if (send_serial_character(COM_CH1, (0x55)) != 0) //U
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)2)
				{
					if (send_serial_character(COM_CH1, (0x50)) != 0) //P
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)3)
				{
					if (send_serial_character(COM_CH1, (0x3a)) != 0) //:
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)4)
				{
					if (send_serial_character(COM_CH1, (0x20)) != 0) //SPACE
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)5)
				{
					if (send_serial_character(COM_CH1, (0x56)) != 0) //V
					{
						printf("Neuspesno slanje na serijsku");
					}
				}
				else if (temp == (uint8_t)6)
				{
					if (send_serial_character(COM_CH1, (vrata + '0')) != 0) //koja vrata
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)7)
				{
					if (send_serial_character(COM_CH1, (0x4f)) != 0) //O
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else if (temp == (uint8_t)8)
				{
					if (send_serial_character(COM_CH1, (0x0d)) != 0) //carriage return
					{
						printf("Neuspesno slanje na serijsku");
					}
				}

				else
				{
					temp = 0;
				}
			}

			else
			{
				serijska = 0;
			}

			if (xQueueSend(Queue_serijska, &serijska, 0) != pdTRUE)
			{
				printf("Slanje podataka u red6 nije uspelo \n");
			}

			if ((select_7seg_digit(0)) != 0)
			{
				printf("Neuspesno selektovanje 7seg");
			}

			if ((set_7seg_digit(hexnum[vrata]) != 0))
			{
				printf("Neuspesno setovanje 7seg");
			}
		}

		if ((select_7seg_digit(0)) != 0)
		{
			printf("Neuspesno selektovanje 7seg");
		}

		if ((set_7seg_digit(hexnum[vrata]) != 0))
		{
			printf("Neuspesno setovanje 7seg");
		}
	}


}

static void Serial_send_senzori(TimerHandle_t timer1) {

	t_point = 0;

	if (t_point > (sizeof(trigger) - 1))
		t_point = 0;
	send_serial_character(COM_CH, trigger[t_point++]);

}

static void Senzori_inf(void* pvParameters) {

	Senzori_struct senzori;
	uint8_t brzina;
	uint8_t buffer_vrata[10];


	for (;;) {

		if (xQueueReceive(Queue_vrata, &buffer_vrata, portMAX_DELAY) != pdTRUE) {
			printf("Preuzimanje podataka iz reda2 nije uspelo");
		}

		if (xQueueReceive(Queue_brzina, &brzina, portMAX_DELAY) != pdTRUE) {
			printf("Preuzimanje podataka iz reda1 nije uspelo");
		}

		senzori.brzina = brzina;
		senzori.vrata[0] = buffer_vrata[0];
		senzori.vrata[1] = buffer_vrata[1];
		senzori.vrata[2] = buffer_vrata[2];
		senzori.vrata[3] = buffer_vrata[3];
		senzori.vrata[4] = buffer_vrata[4];

		if (xQueueSend(Queue_senzori, &senzori, 0) != pdTRUE) {
			printf("Slanje podataka u red nije uspelo \n");
		}




	}
}

static void Serial_receive_tsk_PC(void* pvParameters) {

	uint8_t cc;
	uint8_t poruka;



	for (;;) {


		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) { //znamo da je nesto stiglo od PCa
			printf("Greska prilikom preuzimanja semafora1");
		}
		if (get_serial_character(1, &cc) != 0) {
			printf("Greska prilikom preuzimanja karaktera00");
		}

		poruka = cc;


		if (xQueueSend(Queue_PC, &poruka, 0) != pdTRUE) {
			printf("Slanje podataka u red nije uspelo00 \n");
		}
	}
}

static void Serial_receive_tsk_brzina(void* pvParameters) {

	uint8_t cc = 0;
	uint8_t trenutna_brzina = 0;

	for (;;) {


		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) { //znamo da je nesto stiglo sa senzora
			printf("Greska prilikom preuzimanja semafora0");
		}
		if (get_serial_character(0, &cc) != 0) {
			printf("Greska prilikom preuzimanja karaktera0");
		}

		if ((cc != 0xef) && (cc != 0xff) && (cc != 0xfe) && (cc != 0x30) && (cc != 0x31)) { // START bajt za brzinu je ef a STOP bajt je ff; 
																							 // START bajt za vrata je fe a 0x30 i 0x31 su 
			trenutna_brzina = cc;															//	hex brojevi za ASCII tabelu za brojeve 0 i 1

		}

		if (xQueueSend(Queue_brzina, &trenutna_brzina, 0) != pdTRUE) {
			//printf("Slanje podataka u red nije uspelo0 \n");
		}
	}

}


static void Serial_receive_tsk_vrata(void* pvParameters) {

	uint8_t cc;
	uint8_t pl = 0;
	uint8_t pd = 0;
	uint8_t zl = 0;
	uint8_t zd = 0;
	uint8_t g = 0;

	for (;;) {


		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {//znamo da je nesto stiglo sa senzora
			printf("Greska prilikom preuzimanja semafora");
		}
		if (get_serial_character(0, &cc) != 0) {
			printf("Greska prilikom preuzimanja karaktera");
		}
		if (cc == 0xfe) { //proveravamo da li je pocetak poruke, fe = START bajt za vrata

			r_point = 0;

		}

		else if (cc == 0xff) { //proveravamo da li je kraj poruke, ff = STOP bajt  za vrata 

			pd = r_buffer[0];
			zd = r_buffer[1];
			pl = r_buffer[2];
			zl = r_buffer[3];
			g = r_buffer[4];

			if (xQueueSend(Queue_vrata, &r_buffer, 0) != pdTRUE) {
				printf("Slanje podataka u red nije uspelo0000");


			}


		}

		else if ((r_point < R_BUF_SIZE)) {
			if ((cc != 0xef)) {				//proveravamo da nije u pitanu 0xef tj START bajt za senzor brzine 
				r_buffer[r_point++] = cc;
			}
			else {
				r_buffer[0] = pd;
				r_buffer[1] = zd;
				r_buffer[2] = pl;
				r_buffer[3] = zl;
				r_buffer[4] = g;
			}
		}

	}
}


static uint32_t processRXCInterrupt(void) {

	BaseType_t  rxchighpritaskwoken = pdFALSE;

	if (get_RXC_status(0) != 0) {

		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &rxchighpritaskwoken) != pdTRUE)
		{
			printf("Greska prilikom slanja semafora0");
		}
	}

	if (get_RXC_status(1) != 0) {

		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &rxchighpritaskwoken) != pdTRUE)
		{
			printf("Greska prilikom slanja semafora1");
		}
	}
	portYIELD_FROM_ISR(rxchighpritaskwoken);
}

static void onLEDchangeInterrupt() {

	BaseType_t  highpritaskwoken = pdFALSE;

	xSemaphoreGiveFromISR(LED_int_BinarySemaphore, &highpritaskwoken);
	portYIELD_FROM_ISR(highpritaskwoken);
}

void main_demo(void) {

	//inicijalizacija za serijsku 
	init_serial_uplink(COM_CH);  // inicijalizacija serijske TX na kanalu 0
	init_serial_downlink(COM_CH);// inicijalizacija serijske RX na kanalu 0
	init_serial_downlink(COM_CH1);// inicijalizacija serijske TX na kanalu 1
	init_serial_uplink(COM_CH1);// inicijalizacija serijske TX na kanalu 1

	//inicijalizacija za periferije
	init_LED_comm();
	init_7seg_comm();

	//interapti
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, processRXCInterrupt);

	//semafori
	RXC_BinarySemaphore0 = xSemaphoreCreateBinary();
	RXC_BinarySemaphore1 = xSemaphoreCreateBinary();
	LED_int_BinarySemaphore = xSemaphoreCreateBinary();


	//taskovi
	status = xTaskCreate(Serial_receive_tsk_vrata,
		"RECEIVE VRATA",
		configMINIMAL_STACK_SIZE,
		(void*)0,
		TASK_SERIAl_REC_PRI,
		NULL);
	status = xTaskCreate(Serial_receive_tsk_brzina,
		"RECEIVE BRZINA",
		configMINIMAL_STACK_SIZE,
		(void*)1,
		TASK_SERIAl_REC_PRI,
		NULL);
	status = xTaskCreate(Senzori_inf,
		"Senzori",
		configMINIMAL_STACK_SIZE,
		(void*)2,
		SERVICE_TASK_PRI,
		NULL);
	status = xTaskCreate(Serial_receive_tsk_PC,
		"RECEIVE PC",
		configMINIMAL_STACK_SIZE,
		(void*)3,
		TASK_SERIAl_REC_PRI,
		NULL);
	status = xTaskCreate(Serial_send_PC,
		"Slanje ka PCju",
		configMINIMAL_STACK_SIZE,
		(void*)4,
		TASK_SERIAL_SEND_PRI,
		NULL);
	status = xTaskCreate(Led_bar_tsk,
		"Led_bar",
		configMINIMAL_STACK_SIZE,
		(void*)5,
		SERVICE_TASK_PRI,
		NULL);





	//tajmeri
	timer1 = xTimerCreate("Timer1",
		pdMS_TO_TICKS(200),
		pdTRUE,
		(void*)0,
		Serial_send_senzori);
	xTimerStart(timer1, 0);

	//redovi
	Queue_vrata = xQueueCreate(5, 5U);
	Queue_brzina = xQueueCreate(5, 1U);
	Queue_senzori = xQueueCreate(5, sizeof(Senzori_struct));
	Queue_PC = xQueueCreate(5, 1U);
	Queue_prekidac = xQueueCreate(2, 1U);
	Queue_serijska = xQueueCreate(2, 1U);
	Queue_otvorena_vrata = xQueueCreate(2, 1U);
	Queue_gepek = xQueueCreate(2, 1U);

	vTaskStartScheduler();
	for (;;);

}