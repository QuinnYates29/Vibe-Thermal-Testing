#ifndef CORE_CORE_CONFIG_H
#define CORE_CORE_CONFIG_H

#define PROGRAM_NAME_STRING "Template project"

/***************** CLOCK PARAMETERS ****************************/
/***************************************************************/
#define CORE_CLOCK_USE_HSE 0
#define CORE_CLOCK_HSE_FREQ 24000
#define CORE_CLOCK_SYSCLK_FREQ 160000
#define CORE_CLOCK_HSI_FREQ 16000
#define CORE_CLOCK_PLLP_DIV 12

/***************** ERROR HANDLER PARAMETERS ********************/
/***************************************************************/
#define CORE_ERROR_HANDLER_BLINK_DELAY 200000


/********************** CAN PARAMETERS *************************/
/***************************************************************/
// CAN bitrate in Hz
#define CORE_CAN_BITRATE 1000000

// Number of CAN messages that can be stored in the CAN FreeRTOS queue
#define CORE_CAN_QUEUE_LENGTH 15

// Timeout for waiting on RX queue
#define CORE_CAN_RX_TIMEOUT 0

// Ports and pins for CAN communication
#define CORE_FDCAN1_TX_PORT GPIOA
#define CORE_FDCAN1_TX_PIN  GPIO_PIN_12
#define CORE_FDCAN1_TX_AF   9
#define CORE_FDCAN1_RX_PORT GPIOA
#define CORE_FDCAN1_RX_PIN  GPIO_PIN_11
#define CORE_FDCAN1_RX_AF   9

// Filters
#define CORE_FDCAN1_MAX_STANDARD_FILTER_NUM 28
#define CORE_FDCAN1_MAX_EXTENDED_FILTER_NUM 8

// Auto-retransmission config
#define CORE_FDCAN1_AUTO_RETRANSMISSION 0

// CAN FD config
#define CORE_FDCAN1_USE_FD 0

/********************* SPI PARAMETERS **************************/
/***************************************************************/
#define CORE_SPI1_SCK_PORT  GPIOA
#define CORE_SPI1_SCK_PIN   GPIO_PIN_5
#define CORE_SPI1_SCK_AF    5
#define CORE_SPI1_MISO_PORT GPIOA
#define CORE_SPI1_MISO_PIN  GPIO_PIN_6
#define CORE_SPI1_MISO_AF   5
#define CORE_SPI1_MOSI_PORT GPIOA
#define CORE_SPI1_MOSI_PIN  GPIO_PIN_7
#define CORE_SPI1_MOSI_AF   5
#define CORE_SPI1_DIVIDER   7
#define CORE_SPI1_DATA_SIZE 8
#define CORE_SPI1_MASTER    1

#define CORE_SPI2_SCK_PORT  GPIOB
#define CORE_SPI2_SCK_PIN   GPIO_PIN_13
#define CORE_SPI2_SCK_AF    5
#define CORE_SPI2_MISO_PORT GPIOB
#define CORE_SPI2_MISO_PIN  GPIO_PIN_14
#define CORE_SPI2_MISO_AF   5
#define CORE_SPI2_MOSI_PORT GPIOB
#define CORE_SPI2_MOSI_PIN  GPIO_PIN_15
#define CORE_SPI2_MOSI_AF   5
#define CORE_SPI2_DIVIDER   7
#define CORE_SPI2_DATA_SIZE 8
#define CORE_SPI2_MASTER    1

#define CORE_SPI3_SCK_PORT  GPIOC
#define CORE_SPI3_SCK_PIN   GPIO_PIN_10
#define CORE_SPI3_SCK_AF    6
#define CORE_SPI3_MISO_PORT GPIOC
#define CORE_SPI3_MISO_PIN  GPIO_PIN_11
#define CORE_SPI3_MISO_AF   6
#define CORE_SPI3_MOSI_PORT GPIOC
#define CORE_SPI3_MOSI_PIN  GPIO_PIN_12
#define CORE_SPI3_MOSI_AF   6
#define CORE_SPI3_DIVIDER   7
#define CORE_SPI3_DATA_SIZE 8
#define CORE_SPI3_MASTER    1

#define CORE_SPI4_SCK_PORT  GPIOE
#define CORE_SPI4_SCK_PIN   GPIO_PIN_12
#define CORE_SPI4_SCK_AF    5
#define CORE_SPI4_MISO_PORT GPIOE
#define CORE_SPI4_MISO_PIN  GPIO_PIN_13
#define CORE_SPI4_MISO_AF   5
#define CORE_SPI4_MOSI_PORT GPIOE
#define CORE_SPI4_MOSI_PIN  GPIO_PIN_14
#define CORE_SPI4_MOSI_AF   5
#define CORE_SPI4_DIVIDER   7
#define CORE_SPI4_DATA_SIZE 8
#define CORE_SPI4_MASTER    1


/******************** USART PARAMETERS *************************/
/***************************************************************/
#define CORE_USART_RXBUFLEN 512
#define CORE_USART_RX_TIMEOUT 64

#define CORE_USART_UPRINTF 1
#define CORE_USART_TXBUFLEN 512

#define CORE_USART1_PORT GPIOC
#define CORE_USART1_PINS (GPIO_PIN_4 | GPIO_PIN_5)
#define CORE_USART2_PORT GPIOA
#define CORE_USART2_PINS (GPIO_PIN_3 | GPIO_PIN_2)
#define CORE_USART3_PORT GPIOC
#define CORE_USART3_PINS (GPIO_PIN_10 | GPIO_PIN_11)


/*********************** RTC PARAMETERS ************************/
/***************************************************************/
#define CORE_RTC_CENTURY 2000


/******************** BOOTLOADER PARAMETERS ********************/
/***************************************************************/
#define CORE_BOOT_FDCAN FDCAN2
#define CORE_BOOT_FDCAN_ID 0x004
#define CORE_BOOT_FDCAN_MASTER_ID 0x084
#define CORE_BOOT_FDCAN_BROADCAST_ID 0x7ff


/********************* TIMEOUT PARAMETERS **********************/
/***************************************************************/
#define CORE_TIMEOUT_NUM 0


/******************** TIMESTAMP PARAMETERS *********************/
/***************************************************************/
#define CORE_TIMESTAMP_MSB TIM2

#endif //CORE_CORE_CONFIG_H
