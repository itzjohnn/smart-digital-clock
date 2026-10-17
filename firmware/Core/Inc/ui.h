#ifndef INC_UI_H_
#define INC_UI_H_

#include "main.h"
#include "ds3231.h"
#include "dht11.h"
#include "sh1106.h"

// UI State Machine Modes
typedef enum
{
    UI_STATE_CLOCK = 0,
    UI_STATE_SET_HOURS,
    UI_STATE_SET_MINUTES,
    UI_STATE_SET_MONTH,
    UI_STATE_SET_DAY,
    UI_STATE_SET_YEAR
} UI_State_t;

// UI Initialization
void UI_Init(I2C_HandleTypeDef *hi2c);

// Process single-cycle debounced button edge events
void UI_ProcessButtons(uint8_t btn1_pressed, uint8_t btn2_pressed,
                       uint8_t btn3_pressed, uint8_t btn4_pressed);

// Periodic UI rendering engine (handles both normal clock and edit mode blinking)
void UI_Update(const DS3231_Time_t *current_time, const DHT11_Data_t *climate, HAL_StatusTypeDef dht_status);

// Query active mode
UI_State_t UI_GetState(void);

#endif /* INC_UI_H_ */