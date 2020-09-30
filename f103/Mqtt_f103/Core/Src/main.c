/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"

#include "spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

//INCLUSAO DAS LIBS PARA W5500 E MQTT
#include "socket.h"	// Just include one header for WIZCHIP
#include "wizchip_conf.h"
#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "dhcp.h"
#include "dns.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	Bit_RESET = 0,
	Bit_SET
}BitAction;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//defines para socket e tamanho de buffer para w5500
#define TCP_SOCKET	0
#define UDP_SOCKET	1
#define SOCK_DNS    6
#define DATA_BUF_SIZE   2048

//defines para bits do evt groups
#define BIT_0_MQTT (1 << 0)
#define BIT_1 (1 << 1)
#define BIT_2 (1 << 2)
#define BIT_3 (1 << 3)
#define BIT_4 (1 << 4)
#define BIT_5 (1 << 5)
#define BIT_6 (1 << 6)
#define BIT_7 (1 << 7)
#define BIT_8 (1 << 8)
#define BIT_9 (1 << 9)
#define BIT_10 (1 << 10)
#define BIT_11 (1 << 11)
#define BIT_12 (1 << 12)
#define BIT_13 (1 << 13)
#define BIT_14 (1 << 14)
#define BIT_15 (1 << 15)
#define BIT_16 (1 << 16)
#define BIT_17 (1 << 17)
#define BIT_18 (1 << 18)
#define BIT_19 (1 << 19)
#define BIT_20 (1 << 20)
#define BIT_21 (1 << 21)
#define BIT_22 (1 << 22)
#define BIT_23 (1 << 23)

char local_buf[500] = {0};
int VALUE = 0;
int x = 0;

uint32_t comando;
uint32_t comando1;
uint32_t comando2;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
//mutexes para sicronização de perifericos
SemaphoreHandle_t xMutex = NULL;
SemaphoreHandle_t xSPIW5500_MUTEX = NULL;
EventGroupHandle_t xEvt_Group_f103 = NULL;

//variaveil que sincroniza os timers das libs do mqtt, dns e w5500
unsigned long MilliTimer = 0;

//variaveis para DNS E DHCP
uint16_t SEDHCP =1; //0= IP FIXO //1= DHCP
extern uint8_t  DNS_SOCKET;    // SOCKET number for DNS
uint8_t gDATABUF[DATA_BUF_SIZE];
uint8_t char_buff[500] = {0};

uint16_t porta = 80; //PORTA WEBSERVER
Network n;	//network w5500
Client c;	//client mqtt
//states for multi-thread http
#define HTTP_IDLE 0
#define HTTP_SENDING 1

//variables for multi-thread http
uint32_t sentsize[_WIZCHIP_SOCK_NUM_];
uint32_t filesize[_WIZCHIP_SOCK_NUM_];
uint8_t http_state[_WIZCHIP_SOCK_NUM_];

///////////////////////////////////
// Default Network Configuration //
///////////////////////////////////
wiz_NetInfo gWIZNETINFO = { .mac = {0xDE, 0xAD, 0xBE, 0xFB, 0xFE, 0xED},
							.ip = {169, 254, 216, 240},
							.sn = {255,255,255,0},
							.gw = {169, 254, 216, 1},
							.dns = {0,0,0,0},
							.dhcp = NETINFO_STATIC };

//estrutura de dados para lib do mqtt.
struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
	enum QoS qos;
	char* username;
	char* password;
	char* host;
	int port;
	int showtopics;//       usuário                     password             server      porta
} opts ={(char*)"LucasHardware", 0, (char*)"\n", QOS0, "general", "mqtt", (char*)"5.196.95.208",1883, 1};
				//clientIDMQTTalterado
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
//------TAREFAS------
void vTask_blink (void *pvParameter );
void vMQTT_Yield (void * pvParameter );
void vPub_MQTT ( void *pvParameter );

//-----FUNCOES------
void vPrintString (char* UartSend_f);
void USARTlib_Putc(UART_HandleTypeDef* huart, char c);
void USARTlib_Puts(char* str);
void env_info_rede();
uint8_t ucGet_IP_by_DNS(uint8_t * domain_name, uint8_t * domain_ip_return);
void messageArrived(MessageData* data);
void Publica_MQTT();

//////////
// TODO //
//////////////////////////////////////////////////////////////////////////////////////////////
// Call back function for W5500 SPI - Theses used as parameter of reg_wizchip_xxx_cbfunc()  //
// Should be implemented by WIZCHIP users because host is dependent                         //
//////////////////////////////////////////////////////////////////////////////////////////////
void  wizchip_select(void);
void  wizchip_deselect(void);
void  wizchip_write(uint8_t wb);
uint8_t wizchip_read();
//////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////
// For example of ioLibrary_BSD //
//////////////////////////////////
void network_init(void);								// Initialize Network information and display it
int16_t tcp_client(uint8_t sn, uint8_t* tx_buf, uint8_t* rx_buf, uint8_t buffer_send_size, uint8_t* buffer_recv_size, uint8_t* destip, uint16_t destport);
//////////////////////////////////
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	uint8_t i;
	uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */



  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  //criação de mutexes para sincronização de envio na uart e na spi
  xMutex = xSemaphoreCreateMutex();
  xSPIW5500_MUTEX = xSemaphoreCreateMutex();

  if( xMutex != NULL) vPrintString("xMutex criada com sucesso!\r\n");
  if( xSPIW5500_MUTEX != NULL) vPrintString("xSPIW5500_MUTEX criada com sucesso!\r\n");

	//INICIALIZAÇÃO DO PHY ETH W5500 QUE DEPENDE DAS MUTEX FUNCIONANDO.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// First of all, Should register SPI callback functions implemented by user for accessing WIZCHIP //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/* Chip selection call back */

	#if   _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_VDM_
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
	#elif _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_FDM_
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_select);  // CS must be tried with LOW.
	#else
	#if (_WIZCHIP_IO_MODE_ & _WIZCHIP_IO_MODE_SIP_) != _WIZCHIP_IO_MODE_SIP_
	#error "Unknown _WIZCHIP_IO_MODE_"
	#else
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
	#endif
	#endif

	/* SPI Read & Write callback function */
	reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

	////////////////////////////////////////////////////////////////////////
	/* WIZCHIP SOCKET Buffer initialize */
	if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1)
	{
		//init fail
		while(1);
	}

	/* Network initialization */
	network_init();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	//criação de evt group que sincroniza as tarefas que dependem de um bit
	xEvt_Group_f103 = xEventGroupCreate();

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */


  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */


  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */

  /* USER CODE BEGIN RTOS_THREADS */
	/**
	 * COMO O PROJETO É CONSTITUIDO EM FREERTOS, TEMOS CONSIDERAÇÕES A POR AQUI
	 *
	 * 1 - As tarefas estão concorrendo entre si com uma frequencia de 1KHz, ou seja,
	 * a cada milisegundo ocorre uma troca de contexto e o kernel poe uma nova tarefa em execução,
	 * baseado em nivel de prioridade e se ela esta ou não nos estados, ready, bloqued, suspend.
	 *
	 * 2 - Para enviar dados entre tarefas devem ser utilizados as topologias de eventos baseados em Filas
	 *
	 * 3 - Para sincronizar recursos compartilhados, como por exemplo, envio na SPI, UART, display ou algo do tipo,
	 * devem ser utilizadas as mutexes, como estamos utilizando aqui, uma para serial, outra para SPI.
	 *
	 * 4 - Para sincronizar tarefas com recursos globais, como, tarefas que dependem da conexão com mqtt ativa para funcionar por exemplo,
	 * devemos utilizar event_groups,
	 *
	 * 5 - Para sincronizar eventos entre tarefas ou recursos baseados em flag ou maquina de estados, devemos utilizar task notiy e semaforos.
	 */

	if( (xTaskCreate( vTask_blink, "blink task", configMINIMAL_STACK_SIZE * 3 , NULL, 1, NULL) ) != pdTRUE )
	{
	//sprintf(UartSend,"nao foi possivel alocar tarefa vTask_blink no escalonador");
	//HAL_UART_Transmit(&huart2,(uint8_t*) UartSend, sizeof(UartSend), 10);

	vPrintString("nao foi possivel alocar tarefa vTaskBlink no escalonador");
	}

	if( (xTaskCreate( vMQTT_Yield, "MQTT YIELD", configMINIMAL_STACK_SIZE * 6 , NULL, 3, NULL) ) != pdTRUE )
	{
	//sprintf(UartSend,"nao foi possivel alocar tarefa vTask_blink no escalonador");
	//HAL_UART_Transmit(&huart2,(uint8_t*) UartSend, sizeof(UartSend), 10);

	vPrintString("nao foi possivel alocar tarefa vMQTT_Yield no escalonador");
	}

	if( (xTaskCreate( vPub_MQTT, "MQTT PUB", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL) ) != pdTRUE )
	{
	//sprintf(UartSend,"nao foi possivel alocar tarefa vTask_blink no escalonador");
	//HAL_UART_Transmit(&huart2,(uint8_t*) UartSend, sizeof(UartSend), 10);

	vPrintString("nao foi possivel alocar tarefa vPub_MQTT no escalonador");
	}


	vTaskStartScheduler();

  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIO_W5500_CS_GPIO_Port, GPIO_W5500_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : GPIO_W5500_CS_Pin */
  GPIO_InitStruct.Pin = GPIO_W5500_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIO_W5500_CS_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
//------------------------------------------ INICIO DAS TAREFAS ------------------------------------------------------
void vMQTT_Yield (void * pvParameter )
{
	//inicio da tarefa que da manutenção da conexão mqtt
	vPrintString("vMQTT_YIELD iniciada com sucesso\r\n");

	//variavel que armazena o estado atual da conexão
	int rc = -1;

	unsigned int targetPort = 1883; //PORTA SERVIDOR MQTT

	uint8_t Domain_name[] = "";    		// for Example domain name
	uint8_t Domain_IP[4]  = {192,168,1,142 };               		// Translated IP address by DNS Server

	unsigned char recvBuff[1024] = {0};
	unsigned char sendBuff[1024] = {0};

	//função que retorna o IP do mqtt
	ucGet_IP_by_DNS(Domain_name, Domain_IP);

	for(;;)
	{
		//verifica se RC não recebeu estado de conexão ativa, caso nao tenha recebido, faz a conexão com broker mqtt a partir do socket
		if(rc != SUCCESSS)
		{
			//desconetar algum possivel socket aberto e limpar bit de evt group para MQTT.
			w5500_disconnect(&n);
			xEventGroupClearBits(xEvt_Group_f103, BIT_0_MQTT);

			//preparação da camamda network do w5500 para comunicar-se com a camada do mqtt
			NewNetwork(&n);
			vPrintString("Conectando ao servidor do MQTT por meio de socket!\r\n");
			ConnectNetwork(&n, Domain_IP, targetPort);

			//caso conexão do socket seja bem sucedida, é repassado os buffers para conexão com mqtt
			vPrintString("Conectando ao broker mqtt");
			MQTTClient(&c,&n,20000,sendBuff,1024,recvBuff,1024);
			vTaskDelay(10/portTICK_PERIOD_MS);

			MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
			data.willFlag = 0;
			data.MQTTVersion = 3;
			data.clientID.cstring = opts.clientid;
			data.username.cstring = opts.username;
			data.password.cstring = opts.password;
			data.keepAliveInterval = 60;
			data.cleansession = 1;

			//conexão com protoclo mqtt é tentada, caso seja estabelecida, SUCCESS é chamado para rc
			rc = MQTTConnect(&c, &data);
			opts.showtopics = 1;

			if(rc == SUCCESSS)
			{
				vPrintString("MQTT Conectado com sucesso!!\r\n");
				//liberar evt group, fazer subscriber em topico e etc..
				MQTTSubscribe(&c, "w5500_LUCAS_", QOS0, messageArrived);

				xEventGroupSetBits(xEvt_Group_f103, BIT_0_MQTT);
			}
		}

		//if(rc==SUCCESSS) MQTTYield(&c, 1000); //varredura de protocolo mqtt para receber dados em topicos e fazer manutenção da conexao

		//testar esse tipo de implementação
		rc = MQTTYield(&c, 1000);

		memset(char_buff, 0, sizeof(char_buff));
		sprintf(char_buff,"rc value = %d", rc);
		vPrintString(char_buff);

		//delay para liberar contexto as outras tarefas
		vTaskDelay(100/portTICK_PERIOD_MS);
	}

	//vTaskDelet neste ponto da tarefa é uma boa pratica, mas, o codigo nunca deve chegar aqui.
	vTaskDelete(NULL);
}

void vPub_MQTT ( void *pvParameter )
{
	vPrintString("Tarefa Pub_MQTT Iniciada com Sucesso!\r\n");

	for( ;; )
	{
		//esse event_group mantem a tarefa bloqueada enquanto a conexão MQTT foi estabelecida
		xEventGroupWaitBits(xEvt_Group_f103, BIT_0_MQTT, pdFALSE, pdFALSE, portMAX_DELAY);

		//faz o envio de um dado via mqtt
		Publica_MQTT();
		vPrintString("Loop MQTT PUB\n");

		//bloqueia a terfa por 5 segundos para o proximo envio
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
	//vTaskDelet neste ponto da tarefa é uma boa pratica, mas, o codigo nunca deve chegar aqui.
	vTaskDelete(NULL);
}

void vTask_blink( void *pvParameter )
{
	char buff[300];
	vPrintString("vTaskBlink iniciada!! \r\n");
	//uint16_t size_heap = 0;

    for( ;; )
	{
    	//essa tarefa serve para depurarmos o consumo de memoria ram das nossas tarefas, como tambem verificar a quantidade de heap livre

    	//essa função do vTaskList não deve ser utilizada em produção em campo, pois a mesma não é deterministica
    	vTaskList(buff);

    	vPrintString("\r\nTask-------------State-----Prio------Stack---Num\r\n");
    	vPrintString(buff);
    	vPrintString("\n\n\n");

        memset(buff, 0, sizeof(buff));
        //size_heap = xPortGetMinimumEverFreeHeapSize();
        sprintf(buff,"Free Heap: %d bytes\n\n", xPortGetMinimumEverFreeHeapSize());

        vPrintString(buff);

        vTaskDelay( 10000 / portTICK_PERIOD_MS );

       //vPrintString("Task Blink \r\n");
    }

	//vTaskDelet neste ponto da tarefa é uma boa pratica, mas, o codigo nunca deve chegar aqui.
    vTaskDelete(NULL);
}

//------------------------------------------ FIM DAS TAREFAS ---------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------

//------------------------------------------ INICIO DAS FUNÇÔES ------------------------------------------------------
//Função de callback para MQTT
void messageArrived(MessageData* data)
{
	//tentar passar o minimo de tempo possivel aqui no callback, pois temos 5s de timeout setados em mqtt.yield()
	/*
	printf("Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len, data->topicName->lenstring.data,
		data->message->payloadlen, data->message->payload);
	*/

	memset(char_buff, 0, sizeof(char_buff));

	sprintf(char_buff,"\n%.*s\n", data->message->payloadlen, data->message->payload);
	//sprintf(char_buff,"Msg Recebida:\nTopico = %.*s\nMsg= %.*s\n",data->topicName->lenstring.len, data->topicName->lenstring.data, data->message->payloadlen, data->message->payload);
	vPrintString(char_buff);


	comando = atoi  ( char_buff);

	comando1 = comando;

	if (comando1 == 10)
	{
		vPrintString("mss_ok!!! \r\n");
	}


}

void Publica_MQTT()
{
	MQTTMessage pubmsg;
	pubmsg.qos = QOS0;
	pubmsg.retained = 0;


	x++;

	VALUE = x * 5;

	char local_buf [500];
	sprintf (local_buf, "%02u", VALUE);


	pubmsg.payload = local_buf;
	pubmsg.payloadlen = strlen(local_buf);

	MQTTPublish (&c, "ihm/amperes", &pubmsg);

	if(VALUE >= 100)
	{
		VALUE = 0;
		x = 0;
	}
}

void vPrintString (char * UartSend_f)
{
	xSemaphoreTake( xMutex, portMAX_DELAY );
	{

	USARTlib_Puts(UartSend_f);

	}
	xSemaphoreGive( xMutex );

}

void USARTlib_Putc(UART_HandleTypeDef* huart, char c)
{
	//Envia caracter
	HAL_UART_Transmit(huart, &c, 1, 10);
}

void USARTlib_Puts(char* str)
{
	while (*str)
	{
		USARTlib_Putc(&huart2, *str++);
	}
}

uint8_t W5500_rxtx(uint8_t data)
{
	xSemaphoreTake(xSPIW5500_MUTEX, portMAX_DELAY);

	uint8_t rxdata;
	HAL_SPI_TransmitReceive(&hspi1, &data, &rxdata, 1, 50);

	xSemaphoreGive(xSPIW5500_MUTEX);
	return (rxdata);
}

//////////
// TODO //
/////////////////////////////////////////////////////////////////
// SPI Callback function for accessing WIZCHIP                 //
// WIZCHIP user should implement with your host spi peripheral //
/////////////////////////////////////////////////////////////////
void  wizchip_select(void)
{
	W5500_select();
}

void  wizchip_deselect(void)
{
	W5500_release();
}

void  wizchip_write(uint8_t wb)
{
	W5500_tx(wb);
}

uint8_t wizchip_read()
{
   return W5500_rx();
}
//////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////
// Intialize the network information to be used in WIZCHIP //
/////////////////////////////////////////////////////////////
void network_init(void)
{
   uint8_t tmpstr[6];

	ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);

	ctlwizchip(CW_GET_ID,(void*)tmpstr);
	/*
	do{//check phy status.
		if(ctlwizchip(CW_GET_PHYLINK,(void*)&temp) == -1){
			USARTlib_Puts("Unknown PHY link status.\r\n");
		}
	}while(temp == PHY_LINK_OFF);*/

	if(SEDHCP==0){
		USARTlib_Puts("W5500 IP Fixo\r\n");
		wizchip_setnetinfo(&gWIZNETINFO);
	}
	if(SEDHCP==1){
		USARTlib_Puts("W5500 Rodando DHCP\r\n");
		DHCP_init(7, gDATABUF);
		unsigned char dhcp_ret ;

		do{
			dhcp_ret = DHCP_run();

			if((dhcp_ret == 1) || (dhcp_ret == DHCP_IP_CHANGED)) {
				getIPfromDHCP(gWIZNETINFO.ip);
				getGWfromDHCP(gWIZNETINFO.gw);
				getSNfromDHCP(gWIZNETINFO.sn);
				getDNSfromDHCP(gWIZNETINFO.dns);
				gWIZNETINFO.dhcp = NETINFO_DHCP;
				wizchip_setnetinfo(&gWIZNETINFO);
			}
			if(dhcp_ret == DHCP_FAILED)	{
				USARTlib_Puts(">> DHCP Failed\r\n");
			}
			//USARTlib_Puts("Iteracao para DHCP\r\n");
		}while(dhcp_ret!=4);
	}

	env_info_rede();
}

void env_info_rede(){
	char writeValue[200] = {0};

	wizchip_getnetinfo(&gWIZNETINFO);
	memset(&writeValue[0],0,sizeof(writeValue));
	sprintf(writeValue,"Mac address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	USARTlib_Puts(writeValue);

	memset(&writeValue[0],0,sizeof(writeValue));
	sprintf(writeValue,"IP address: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	USARTlib_Puts(writeValue);

	memset(&writeValue[0],0,sizeof(writeValue));
	sprintf(writeValue,"SM Mask: %d.%d.%d.%d\r\n",gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	USARTlib_Puts(writeValue);

	memset(&writeValue[0],0,sizeof(writeValue));
	sprintf(writeValue,"Gate way: %d.%d.%d.%d\r\n",gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	USARTlib_Puts(writeValue);

	memset(&writeValue[0],0,sizeof(writeValue));
	sprintf(writeValue,"DNS Server: %d.%d.%d.%d\r\n",gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
	USARTlib_Puts(writeValue);
}

uint8_t ucGet_IP_by_DNS(uint8_t * domain_name, uint8_t * domain_ip_return)
{

	uint8_t DNS_1nd[4]    = {8, 8, 8, 8};      	// Secondary DNS server IP
	uint8_t DNS_2nd[4]    = {208, 67, 222, 222};      	// Secondary DNS server IP
	uint8_t DNS_3nd[4]    = {208, 67, 222, 222};      	// Secondary DNS server IP

	uint8_t ret = 0;
	DNS_init(SOCK_DNS, gDATABUF);

	if ((ret = DNS_run(gWIZNETINFO.dns, domain_name, domain_ip_return)) > 0) // try to 1st DNS
	{
	   vPrintString("> 1st DNS Respond\r\n");
	}
	else if ((ret != -1) && ((ret = DNS_run(DNS_1nd, domain_name, domain_ip_return))>0))     // retry to 2nd DNS
	{
		vPrintString("> 2nd DNS Respond\r\n");
	}
	else if(ret == -1)
	{
		vPrintString("> MAX_DOMAIN_NAME is too small. Should be redefined it.\r\n");
	}
	else
	{
		vPrintString("> DNS Failed\r\n");
	}

	if(ret > 0)
	{
		char buff_trans[200];
		sprintf(buff_trans,"> Translated %s to [%d.%d.%d.%d]\r\n",domain_name,domain_ip_return[0],domain_ip_return[1],domain_ip_return[2],domain_ip_return[3]);
		vPrintString(buff_trans);

		return 1;
	}

	return 0;
}




//Código de erros e mensagens:
//sn 	- 	Success : The socket number @b 'sn' passed as parameter
//1	-	SOCK_OK
//-1	-	SOCKERR_SOCKNUM  - Invalid socket number
//-5	-	SOCKERR_SOCKMODE - Invalid operation in the socket
//-13	-	SOCKERR_TIMEOUT  - Timeout occurred
//-7	-	SOCKERR_SOCKSTATUS - Invalid socket status for socket operation
//-14	-	SOCKERR_DATALEN    - zero data length
//-12	-	SOCKERR_IPINVALID   - Wrong server IP address
//-11	-	SOCKERR_PORTZERO    - Server port zero
//-4	-	SOCKERR_SOCKCLOSED  - Socket unexpectedly closed
//-6	-	SOCKERR_SOCKFLAG    - Invaild socket flag
//-1000 -	SOCK_FATAL  - Result is fatal error about socket process.
//0	-	SOCK_BUSY		 - Socket is busy
//200	-	SUCCESS_RECEIVE
int16_t tcp_client(uint8_t sn, uint8_t* tx_buf, uint8_t* rx_buf, uint8_t buffer_send_size, uint8_t* buffer_recv_size, uint8_t* destip, uint16_t destport)
{
   int32_t ret; // return value for SOCK_ERRORs
   uint16_t size = 0;
   // Destination (TCP Server) IP info (will be connected)
   // >> loopback_tcpc() function parameter
   // >> Ex)
   //	uint8_t destip[4] = 	{192, 168, 0, 214};
   //	uint16_t destport = 	5000;

   // Port number for TCP client (will be increased)
   uint16_t any_port = 	502;
   static uint8_t flagSent = 0;

   // Socket Status Transitions
   // Check the W5500 Socket n status register (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED:
         if(getSn_IR(sn) & Sn_IR_CON)	// Socket n interrupt register mask; TCP CON interrupt = connection with peer is successful
         {
#ifdef _LOOPBACK_DEBUG_
			printf("%d:Connected to - %d.%d.%d.%d : %d\r\n",sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
			setSn_IR(sn, Sn_IR_CON);  // this interrupt should be write the bit cleared to '1'
         }

         //////////////////////////////////////////////////////////////////////////////////////////////
         // Data Transaction Parts; Handle the [data receive and send] process
         //////////////////////////////////////////////////////////////////////////////////////////////
         if(flagSent == 0)
         {
        	 flagSent = 1;
    		 ret = send(sn, tx_buf, buffer_send_size); // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
    		 if(ret < 0) // Send Error occurred (sent data length < 0)
    		 {
    			 close(sn); // socket close
    			 flagSent = 0;
    			 return ret;
    		 }
         }
         else if((size = getSn_RX_RSR(sn)) > 0) // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
         {

			ret = recv(sn, rx_buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)

			if(ret <= 0) return ret; // If the received data length <= 0, receive failed and process end
			*buffer_recv_size = size;
			flagSent = 0;
			return 200; //Success receive
         }

		 //////////////////////////////////////////////////////////////////////////////////////////////
         break;

      case SOCK_CLOSE_WAIT :
#ifdef _LOOPBACK_DEBUG_
         //printf("%d:CloseWait\r\n",sn);
#endif
         if((ret=disconnect(sn)) != SOCK_OK) return ret;
#ifdef _LOOPBACK_DEBUG_
         printf("%d:Socket Closed\r\n", sn);
#endif
         break;

      case SOCK_INIT :
#ifdef _LOOPBACK_DEBUG_
    	 printf("%d:Try to connect to the %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
    	 if( (ret = connect(sn, destip, destport)) != SOCK_OK) return ret;	//	Try to TCP connect to the TCP server (destination)
         break;

      case SOCK_CLOSED:
    	  close(sn);
    	  if((ret=socket(sn, Sn_MR_TCP, any_port++, 0x00)) != sn) return ret; // TCP socket open with 'any_port' port number
#ifdef _LOOPBACK_DEBUG_
    	 //printf("%d:TCP client loopback start\r\n",sn);
         //printf("%d:Socket opened\r\n",sn);
#endif
         break;
      default:
         break;
   }
   return 1;
}

//------------------------------------------ FIM DAS FUNÇÔES ------------------------------------------------------
/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */


  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4) {
	MilliTimer++;
	if(MilliTimer % 1000 == 0)	DHCP_time_handler();
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
