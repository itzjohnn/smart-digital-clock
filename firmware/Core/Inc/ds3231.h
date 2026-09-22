#ifndef INC_DS3231_H_
#define INC_DS3231_H_

#include "main.h"

// 8-bit shifted I2C address and starting register offset
#define DS3231_I2C_ADDR     (0x68 << 1) // 7-bit 0x68 shifted
#define DS3231_REG_TIME     0x00        // seconds register (start of burst read/write)

// Structured storage container holding decoded human-readable time and date values
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;        // 00-23 (24-hour mode)
    uint8_t day_of_week;  // 1 = Sunday, 2 = Monday, ... 7 = Saturday
    uint8_t day_of_month; // day of month (1-31)
    uint8_t month;        // month of the year (1-12)
    uint8_t year;         // year (00-99)
} DS3231_Time_t;

// Public Driver APIs
// Configure initial clock time or retrieve live time snapshots
HAL_StatusTypeDef DS3231_SetTime(I2C_HandleTypeDef *hi2c, const DS3231_Time_t *time);
HAL_StatusTypeDef DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time);

#endif