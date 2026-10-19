#include "ui.h"
#include <stdio.h>

// 3-letter weekday lookup table (0 = SUN, 1 = MON, ..., 6 = SAT)
static const char *const DAY_NAMES[] =
{
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

// Sakamoto's algorithm for calculating day of week
// Returns: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
static uint8_t UI_CalculateDayOfWeek(uint8_t day, uint8_t month, uint16_t year)
{
    static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3)
    {
        year -= 1;
    }
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}


// Hardware interface handle
static I2C_HandleTypeDef *ui_i2c = NULL;

// Active state machine tracking
static UI_State_t ui_state = UI_STATE_CLOCK;
static DS3231_Time_t edit_time;

// Visual cursor blink tracking for edit mode
static uint32_t last_blink_tick = 0;
static uint8_t blink_state = 1;
static int8_t last_second = -1;

void UI_Init(I2C_HandleTypeDef *hi2c)
{
    ui_i2c = hi2c;
    ui_state = UI_STATE_CLOCK;
    blink_state = 1;
    last_second = -1;
}

UI_State_t UI_GetState(void)
{
    return ui_state;
}

void UI_ProcessButtons(uint8_t btn1_pressed, uint8_t btn2_pressed,
                       uint8_t btn3_pressed, uint8_t btn4_pressed)
{
    // Button 1: Mode Cycle / Next Field
    if (btn1_pressed)
    {
        if (ui_state == UI_STATE_CLOCK)
        {
            // Enter edit mode: freeze live time into edit buffer
            DS3231_GetTime(ui_i2c, &edit_time);
            ui_state = UI_STATE_SET_HOURS;
        }
        else if (ui_state == UI_STATE_SET_HOURS)
        {
            ui_state = UI_STATE_SET_MINUTES;
        }
        else if (ui_state == UI_STATE_SET_MINUTES)
        {
            ui_state = UI_STATE_SET_MONTH;
        }
        else if (ui_state == UI_STATE_SET_MONTH)
        {
            ui_state = UI_STATE_SET_DAY;
        }
        else if (ui_state == UI_STATE_SET_DAY)
        {
            ui_state = UI_STATE_SET_YEAR;
        }
        else if (ui_state == UI_STATE_SET_YEAR)
        {
            ui_state = UI_STATE_SET_HOURS; // Wrap around edit fields
        }
        blink_state = 1;
        last_blink_tick = HAL_GetTick();
    }

    // Button 2: Increment (+1)
    if (btn2_pressed && ui_state != UI_STATE_CLOCK)
    {
        switch (ui_state)
        {
            case UI_STATE_SET_HOURS:
                edit_time.hours = (edit_time.hours + 1) % 24;
                break;
            case UI_STATE_SET_MINUTES:
                edit_time.minutes = (edit_time.minutes + 1) % 60;
                break;
            case UI_STATE_SET_MONTH:
                edit_time.month = (edit_time.month >= 12) ? 1 : edit_time.month + 1;
                break;
            case UI_STATE_SET_DAY:
                edit_time.day_of_month = (edit_time.day_of_month >= 31) ? 1 : edit_time.day_of_month + 1;
                break;
            case UI_STATE_SET_YEAR:
                edit_time.year = (edit_time.year + 1) % 100;
                break;
            default:
                break;
        }
        blink_state = 1;
        last_blink_tick = HAL_GetTick();
    }

    // Button 3: Decrement (-1)
    if (btn3_pressed && ui_state != UI_STATE_CLOCK)
    {
        switch (ui_state)
        {
            case UI_STATE_SET_HOURS:
                edit_time.hours = (edit_time.hours == 0) ? 23 : edit_time.hours - 1;
                break;
            case UI_STATE_SET_MINUTES:
                edit_time.minutes = (edit_time.minutes == 0) ? 59 : edit_time.minutes - 1;
                break;
            case UI_STATE_SET_MONTH:
                edit_time.month = (edit_time.month <= 1) ? 12 : edit_time.month - 1;
                break;
            case UI_STATE_SET_DAY:
                edit_time.day_of_month = (edit_time.day_of_month <= 1) ? 31 : edit_time.day_of_month - 1;
                break;
            case UI_STATE_SET_YEAR:
                edit_time.year = (edit_time.year == 0) ? 99 : edit_time.year - 1;
                break;
            default:
                break;
        }
        blink_state = 1;
        last_blink_tick = HAL_GetTick();
    }

    // Button 4: Save & Exit in Edit Mode, Tactile Beep in Clock Mode
    if (btn4_pressed)
    {
        if (ui_state != UI_STATE_CLOCK)
        {
            // Reset seconds to 00 and calculate 1-indexed day of week (1 = Sunday)
            edit_time.seconds = 0;
            edit_time.day_of_week = UI_CalculateDayOfWeek(edit_time.day_of_month, 
                                                          edit_time.month, 
                                                          2000 + edit_time.year) + 1;
            DS3231_SetTime(ui_i2c, &edit_time);
            ui_state = UI_STATE_CLOCK;

            // Confirmation beep
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
            HAL_Delay(60);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
        }
        else
        {
            // Tactile feedback beep in normal clock mode
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
            HAL_Delay(30);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
        }
    }
}

void UI_Update(const DS3231_Time_t *current_time, const DHT11_Data_t *climate, HAL_StatusTypeDef dht_status)
{
    char time_str[16];
    char date_str[16];
    char climate_str[20];

    if (ui_state == UI_STATE_CLOCK)
    {
        // Only refresh the OLED when the second tick changes
        if (current_time->seconds != last_second)
        {
            last_second = current_time->seconds;

            uint8_t dow = UI_CalculateDayOfWeek(current_time->day_of_month, 
                                                 current_time->month, 
                                                 2000 + current_time->year);

            sprintf(time_str, "%02d:%02d:%02d", current_time->hours, current_time->minutes, current_time->seconds);
            sprintf(date_str, "%s  %02d/%02d/%02d", DAY_NAMES[dow], current_time->month, current_time->day_of_month, current_time->year);

            if (dht_status == HAL_OK)
            {
                uint8_t temp_f = (climate->temperature * 9 / 5) + 32;
                sprintf(climate_str, "%d\x7F" "F  %d%%RH", temp_f, climate->humidity);
            }
            else
            {
                sprintf(climate_str, "--\x7F" "F  --%%RH");
            }

            SH1106_Clear();

            // Row 1: Time (Centered)
            SH1106_SetCursor(36, 4);
            SH1106_WriteString(time_str, Font_7x10, SH1106_COLOR_WHITE);

            // Row 2: Date (Centered)
            SH1106_SetCursor(18, 22);
            SH1106_WriteString(date_str, Font_7x10, SH1106_COLOR_WHITE);

            // Row 3: Climate Sensor Readings (Centered)
            SH1106_SetCursor(25, 42);
            SH1106_WriteString(climate_str, Font_7x10, SH1106_COLOR_WHITE);

            SH1106_UpdateScreen(ui_i2c);
        }
    }
    else
    {
        // EDIT MODE: blink active field every 300 ms
        if (HAL_GetTick() - last_blink_tick >= 300)
        {
            last_blink_tick = HAL_GetTick();
            blink_state = !blink_state;
        }

        // Format Time string with blinking field
        char hrs_buf[4], mins_buf[4];
        if (ui_state == UI_STATE_SET_HOURS && !blink_state)
            sprintf(hrs_buf, "  ");
        else
            sprintf(hrs_buf, "%02d", edit_time.hours);

        if (ui_state == UI_STATE_SET_MINUTES && !blink_state)
            sprintf(mins_buf, "  ");
        else
            sprintf(mins_buf, "%02d", edit_time.minutes);

        sprintf(time_str, "%s:%s:--", hrs_buf, mins_buf);

        // Format Date string with blinking field
        char mo_buf[4], day_buf[4], yr_buf[4];
        if (ui_state == UI_STATE_SET_MONTH && !blink_state)
            sprintf(mo_buf, "  ");
        else
            sprintf(mo_buf, "%02d", edit_time.month);

        if (ui_state == UI_STATE_SET_DAY && !blink_state)
            sprintf(day_buf, "  ");
        else
            sprintf(day_buf, "%02d", edit_time.day_of_month);

        if (ui_state == UI_STATE_SET_YEAR && !blink_state)
            sprintf(yr_buf, "  ");
        else
            sprintf(yr_buf, "%02d", edit_time.year);

        uint8_t edit_dow = UI_CalculateDayOfWeek(edit_time.day_of_month, 
                                                edit_time.month, 
                                                2000 + edit_time.year);
            
        sprintf(date_str, "%s  %s/%s/%s", DAY_NAMES[edit_dow], mo_buf, day_buf, yr_buf);

        // Render Edit Mode UI
        SH1106_Clear();

        SH1106_SetCursor(36, 4);
        SH1106_WriteString(time_str, Font_7x10, SH1106_COLOR_WHITE);

        SH1106_SetCursor(18, 22);
        SH1106_WriteString(date_str, Font_7x10, SH1106_COLOR_WHITE);

        SH1106_SetCursor(22, 42);
        SH1106_WriteString("[SET TIME]", Font_7x10, SH1106_COLOR_WHITE);

        SH1106_UpdateScreen(ui_i2c);
    }
}