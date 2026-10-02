#include "dht11.h"

// Microsecond delay using ARM SysTick counter
static void delay_us(uint32_t us)
{
    // SystemCoreClock runs at 48 MHz (48 ticks per microsecond)
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t start = SysTick->VAL;

    while (1)
    {
        uint32_t current = SysTick->VAL;
        uint32_t elapsed = (start >= current) ? (start - current) : (SysTick->LOAD - current + start);
        if (elapsed >= ticks)
        {
            break;
        }
    }
}

// Configures PA2 as Push-Pull Output
static void DHT11_SetPinOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// Configures PA2 as Digital Input with pull-up
static void DHT11_SetPinInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// Initializes DHT11 pin to idel HIGH state
void DHT11_Init(void)
{
    DHT11_SetPinOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
}

// Reads 40-bit climate packet from DHT11
HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *data)
{
    uint8_t raw_data[5] = {0};
    uint32_t timeout = 0;

    // Host sends Start Signal
    DHT11_SetPinOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(18); // Pull LOW for >= 18 ms

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30); // Pull HIGH for 20-40 us

    // Switch pin to Input and wait for DHT11 response
    DHT11_SetPinInput();

    // Wait for DHT11 to pull pin LOW (response signal start, ~80 us)
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        if (--timeout == 0) return HAL_TIMEOUT;
    }

    // Wait for DHT11 to pull pin HIGH (response signal end, ~80 us)
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
    {
        if (--timeout == 0) return HAL_TIMEOUT;
    }

    // Wait for DHT11 to drop LOW (start of 40-bit data transmission)
    timeout = 10000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        if (--timeout == 0) return HAL_TIMEOUT;
    }

    // Read 40 bits (5 bytes)
    for (int i = 0; i < 40; i++)
    {
        // Each bit starts with a 50 us LOW pulse
        timeout = 10000;
        while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
        {
            if (--timeout == 0) return HAL_TIMEOUT;
        }

        // Pin is now HIGH
        // Delay 40 us and check if line is still HIGH
        // 26-28 us HIGH = bit '0', 70 us HIGH = bit '1'
        delay_us(40);

        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
        {
            // Still HIGH after 40 us -> Bit is 1
            raw_data[i / 8] |= (1 << (7 - (i % 8)));

            // Wait for pin to fall back LOW before next bit
            timeout = 10000;
            while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            {
                if (--timeout == 0) return HAL_TIMEOUT;
            }
        }
    }

// Verify Checksum: Byte4 = (Byte0 + Byte1 + Byte2 + Byte3) & 0xFF
    uint8_t sum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (sum != raw_data[4])
    {
        return HAL_ERROR;
    }

    // Store decoded measurements
    data->humidity    = raw_data[0];
    data->temperature = raw_data[2];

    return HAL_OK;
}