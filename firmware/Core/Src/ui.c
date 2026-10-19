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

// Alarm instance (default 7:00, disabled)
static Alarm_t alarm =
{
    .hours = 7,
    .minutes = 0,
    .enabled = 0,
    .triggered = 0
};

// Working edit buffer for alarm setup
static Alarm_t edit_alarm;

// Buzzer cadence tracking (non-blocking)
static uint32_t last_buzzer_tick = 0;
static uint8_t buzzer_cadence_state = 0;
static int8_t last_alarm_trigger_min = -1;

void UI_Init(I2C_HandleTypeDef *hi2c)
{
    ui_i2c = hi2c;
    ui_state = UI_STATE_CLOCK;
    blink_state = 1;
    last_second = -1;
    edit_alarm = alarm;
}

UI_State_t UI_GetState(void)
{
    return ui_state;
}

uint8_t UI_IsAlarmTriggered(void)
{
    return alarm.triggered;
}

// Checks if current time matches alarm target (evaluates on second 00)
void UI_CheckAlarm(const DS3231_Time_t *current_time)
{
    if (alarm.enabled && !alarm.triggered)
    {
        if (current_time->seconds == 0 &&
            current_time->hours == alarm.hours &&
            current_time->minutes == alarm.minutes &&
            current_time->minutes != last_alarm_trigger_min)
        {
            alarm.triggered = 1;
            last_alarm_trigger_min = current_time->minutes;
            last_buzzer_tick = HAL_GetTick();
            buzzer_cadence_state = 1;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
        }
    }
}

// Non-blocking buzzer cadence: 150 ms ON, 150 ms OFF
void UI_ServiceAlarmBuzzer(void)
{
    if (alarm.triggered)
    {
        if (HAL_GetTick() - last_buzzer_tick >= 150)
        {
            last_buzzer_tick = HAL_GetTick();
            buzzer_cadence_state = !buzzer_cadence_state;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, buzzer_cadence_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
    }
}

void UI_ProcessButtons(uint8_t btn1_pressed, uint8_t btn2_pressed,
                       uint8_t btn3_pressed, uint8_t btn4_pressed)
{
    // Any button press silences an active alarm
    if (alarm.triggered && (btn1_pressed || btn2_pressed || btn3_pressed || btn4_pressed))
    {
        alarm.triggered = 0;
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
        return;
    }

    // Button 1: Mode Cycle / Next Field
    if (btn1_pressed)
    {
        if (ui_state == UI_STATE_CLOCK)
        {
            // Enter edit mode: freeze live time into edit buffer
            DS3231_GetTime(ui_i2c, &edit_time);
            edit_alarm = alarm;
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
            ui_state = UI_STATE_SET_ALARM_HOURS;
        }
        else if (ui_state == UI_STATE_SET_ALARM_HOURS)
        {
            ui_state = UI_STATE_SET_ALARM_MINUTES;
        }
        else if (ui_state == UI_STATE_SET_ALARM_MINUTES)
        {
            ui_state = UI_STATE_SET_ALARM_ENABLE;
        }
        else if (ui_state == UI_STATE_SET_ALARM_ENABLE)
        {
            ui_state = UI_STATE_SET_HOURS; // Wrap around to time edit
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
            case UI_STATE_SET_ALARM_HOURS:
                edit_alarm.hours = (edit_alarm.hours + 1) % 24;
                break;
            case UI_STATE_SET_ALARM_MINUTES:
                edit_alarm.minutes = (edit_alarm.minutes + 1) % 60;
                break;
            case UI_STATE_SET_ALARM_ENABLE:
                edit_alarm.enabled = !edit_alarm.enabled;
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
            case UI_STATE_SET_ALARM_HOURS:
                edit_alarm.hours = (edit_alarm.hours == 0) ? 23 : edit_alarm.hours - 1;
                break;
            case UI_STATE_SET_ALARM_MINUTES:
                edit_alarm.minutes = (edit_alarm.minutes == 0) ? 59 : edit_alarm.minutes - 1;
                break;
            case UI_STATE_SET_ALARM_ENABLE:
                edit_alarm.enabled = !edit_alarm.enabled;
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

            // Commit alarm settings
            alarm = edit_alarm;
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
        // When alarm is triggered, refresh on every tick for alert feedback
        if (current_time->seconds != last_second || alarm.triggered)
        {
            last_second = current_time->seconds;

            uint8_t dow = UI_CalculateDayOfWeek(current_time->day_of_month, 
                                                 current_time->month, 
                                                 2000 + current_time->year);
            
            // Show alarm indicator if enabled
            if (alarm.enabled)
            {
                sprintf(time_str, "%02d:%02d:%02d *", current_time->hours, current_time->minutes, current_time->seconds);
            }
            else
            {
            sprintf(time_str, "%02d:%02d:%02d", current_time->hours, current_time->minutes, current_time->seconds);
            }

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
            uint8_t time_x = alarm.enabled ? 30 : 36;
            SH1106_SetCursor(time_x, 4);
            SH1106_WriteString(time_str, Font_7x10, SH1106_COLOR_WHITE);

            // Row 2: Date (Centered)
            SH1106_SetCursor(18, 22);
            SH1106_WriteString(date_str, Font_7x10, SH1106_COLOR_WHITE);

            // Row 3: Climate Sensor Readings or Alarm Alert (Centered)
            if (alarm.triggered)
            {
                SH1106_SetCursor(22, 42);
                SH1106_WriteString("! ALARM RING !", Font_7x10, SH1106_COLOR_WHITE);
            }
            else
            {
                SH1106_SetCursor(25, 42);
                SH1106_WriteString(climate_str, Font_7x10, SH1106_COLOR_WHITE);
            }

            SH1106_UpdateScreen(ui_i2c);
        }
    }
    else if (ui_state <= UI_STATE_SET_YEAR)
    {
        // CLOCK EDIT MODE (Time and Date): blink active field every 300 ms
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
    else
    {
        // Alarm Edit Mode
        if (HAL_GetTick() - last_blink_tick >= 300)
        {
            last_blink_tick = HAL_GetTick();
            blink_state = !blink_state;
        }

        char a_hrs_buf[4], a_mins_buf[4], a_en_buf[6];
        if (ui_state == UI_STATE_SET_ALARM_HOURS && !blink_state)
            sprintf(a_hrs_buf, "  ");
        else
            sprintf(a_hrs_buf, "%02d", edit_alarm.hours);

        if (ui_state == UI_STATE_SET_ALARM_MINUTES && !blink_state)
            sprintf(a_mins_buf, "  ");
        else
            sprintf(a_mins_buf, "%02d", edit_alarm.minutes);

        if (ui_state == UI_STATE_SET_ALARM_ENABLE && !blink_state)
            sprintf(a_en_buf, "   ");
        else
            sprintf(a_en_buf, edit_alarm.enabled ? "ON " : "OFF");

        sprintf(time_str, "ALM: %s:%s", a_hrs_buf, a_mins_buf);
        sprintf(date_str, "STATE: %s", a_en_buf);

        SH1106_Clear();
        SH1106_SetCursor(25, 4);
        SH1106_WriteString(time_str, Font_7x10, SH1106_COLOR_WHITE);

        SH1106_SetCursor(29, 22);
        SH1106_WriteString(date_str, Font_7x10, SH1106_COLOR_WHITE);

        SH1106_SetCursor(18, 42);
        SH1106_WriteString("[SET ALARM]", Font_7x10, SH1106_COLOR_WHITE);

        SH1106_UpdateScreen(ui_i2c);
    }
}