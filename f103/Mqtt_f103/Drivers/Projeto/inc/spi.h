#include "main.h"

#define W5500_select() HAL_GPIO_WritePin(GPIO_W5500_CS_GPIO_Port, GPIO_W5500_CS_Pin, GPIO_PIN_RESET);
#define W5500_release() HAL_GPIO_WritePin(GPIO_W5500_CS_GPIO_Port, GPIO_W5500_CS_Pin, GPIO_PIN_SET);

#define W5500_rx() W5500_rxtx(0xff)
#define W5500_tx(data) W5500_rxtx(data)

uint8_t W5500_rxtx(uint8_t data);
