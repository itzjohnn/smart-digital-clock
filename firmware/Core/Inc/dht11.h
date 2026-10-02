#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include "main.h"


#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_PIN_2

typedef struct
{
    uint8_t temperature; // Temperature in Celsius 
    uint8_t humidity;    // Relative humidity in %
} DHT11_Data_t;

// API Prototypes
void DHT11_Init(void);
HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data);

#endif