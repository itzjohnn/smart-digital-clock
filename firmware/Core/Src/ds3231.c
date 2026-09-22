#include "ds3231.h"

// Converts Binary Coded Decimal (BCD) from DS3231 registers to standard decimal
static uint8_t BCD2DEC(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Converts standard decimal numbers into BCD format expected by the DS3231
static uint8_t DEC2BCD(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

// Writes date/time parameters to DS3231 registers starting at address 0x00
HAL_StatusTypeDef DS3231_SetTime(I2C_HandleTypeDef *hi2c, const DS3231_Time_t *time)
{
    uint8_t buffer[7];

    buffer[0] = DEC2BCD(time->seconds);
    buffer[1] = DEC2BCD(time->minutes);
    buffer[2] = DEC2BCD(time->hours);         // 24-hour mode
    buffer[3] = DEC2BCD(time->day_of_week);
    buffer[4] = DEC2BCD(time->day_of_month);
    buffer[5] = DEC2BCD(time->month);
    buffer[6] = DEC2BCD(time->year);

    // Transmit 7 consecutive bytes via I2C starting at register 0x00
    return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDR, DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT, buffer, sizeof(buffer), 100);
}

// Reads current date/time registers from the DS3231 and decodes them into decimal values
HAL_StatusTypeDef DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time)
{
    uint8_t buffer[7] = {0};

    // Burst-read 7 consecutive registers from the DS3231 starting at register 0x00
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, DS3231_I2C_ADDR, DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT, buffer, sizeof(buffer), 100);

    // If transmission succeeded, mask out control bits and decode BCD into decimal
    if (status == HAL_OK)
    {
        time->seconds       = BCD2DEC(buffer[0] & 0x7F);
        time->minutes       = BCD2DEC(buffer[1] & 0x7F);
        time->hours         = BCD2DEC(buffer[2] & 0x3F); // Masks out 12/24-hour selection flags
        time->day_of_week   = BCD2DEC(buffer[3] & 0x07);
        time->day_of_month  = BCD2DEC(buffer[4] & 0x3F);
        time->month         = BCD2DEC(buffer[5] & 0x1F); // Masks out the century flag
        time->year          = BCD2DEC(buffer[6]);
    }
    return status;
}