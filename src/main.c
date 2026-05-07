#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "pins.h"
#include "app_const.h"

static TaskHandle_t pwm_task_handle = NULL;
static TaskHandle_t intr_task_handle = NULL;

int directionMap[] = {20, -100, 20, -20, 0, 10, 0, 10};
//  { 360, -360x5, 360, -360, 2sec wait, 180, 2sec wait, 180 }

int currentRouterTicks = 0;
int currentDirectionIndex = 0;

enum MotorDirection dcMotorDirection = MOTOR_STOP;

void runNextDirectionCommand() {
    currentDirectionIndex = (currentDirectionIndex + 1) % (sizeof(directionMap) / sizeof(directionMap[0]));

    currentRouterTicks = directionMap[currentDirectionIndex];

    if (currentRouterTicks > 0) {
        dcMotorDirection = MOTOR_FORWARD;
    } else if (currentRouterTicks < 0) {
        dcMotorDirection = MOTOR_BACKWARD;
    } else {
        dcMotorDirection = MOTOR_STOP;
    }

    xTaskNotifyGive(pwm_task_handle);
}

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(intr_task_handle, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void init_pwm() {
    ledc_timer_config_t lt = {
        .speed_mode = LEDC_LOW_SPEED_MODE, 
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT, 
        .freq_hz = 20000, // 20 kHz 
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&lt);

    uint8_t pins[] = {GPIO_MOTOR_PIN_1, GPIO_MOTOR_PIN_2};
    for (int i = 0; i < 2; i++) {
        ledc_channel_config_t lc = {
            .speed_mode = LEDC_LOW_SPEED_MODE, .channel = i,
            .timer_sel = LEDC_TIMER_0, .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = pins[i], .duty = 0, .hpoint = 0
        };
        ledc_channel_config(&lc);
    }
}

static void pwm_switcher_task(void *param) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int motor1_active = dcMotorDirection == MOTOR_FORWARD ? 1 : 0;
        int motor2_active = dcMotorDirection == MOTOR_BACKWARD ? 1 : 0;

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, motor1_active * DUTY_75);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, motor2_active * DUTY_75);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        if (dcMotorDirection == MOTOR_STOP) {
            vTaskDelay(pdMS_TO_TICKS(2000));

            runNextDirectionCommand();
        }
    }
}

static void interrupt_task(void *param) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        int direction = gpio_get_level(GPIO_ENCODER_DIRECTION_PIN);

        ESP_LOGI(
            TAG, 
            "direction: %d, dcMotorDirection: %d, currentRouterTicks: %d, currentDirectionIndex: %d",
            direction, dcMotorDirection, currentRouterTicks, currentDirectionIndex
        );

        currentRouterTicks = direction ? (currentRouterTicks + 1) : (currentRouterTicks - 1);

        if (currentRouterTicks == 0) {
            runNextDirectionCommand();
        }
    }
}

void motorTaskStart() {
    currentDirectionIndex = 0;
    currentRouterTicks = directionMap[currentDirectionIndex];    

    dcMotorDirection = currentRouterTicks > 0 ? MOTOR_FORWARD : MOTOR_BACKWARD;

    xTaskNotifyGive(pwm_task_handle);
}

void setupAndActivateEncoder() {
    gpio_config_t out_conf = { 
        .pin_bit_mask = (1ULL << GPIO_ENCODER_PLUS), 
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&out_conf);
    gpio_set_level(GPIO_ENCODER_PLUS, 1);

    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << GPIO_ENCODER_CLK_PIN) | (1ULL << GPIO_ENCODER_DIRECTION_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1, 
        .intr_type = GPIO_INTR_NEGEDGE
    };

    gpio_config(&in_conf);
}

void app_main() {
    setupAndActivateEncoder();

    init_pwm();

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_ENCODER_CLK_PIN, gpio_isr_handler, NULL);

    xTaskCreate(pwm_switcher_task, "PWM_Task", 3072, NULL, 2, &pwm_task_handle);
    xTaskCreate(interrupt_task, "Intr_Task", 3072, NULL, 3, &intr_task_handle);

    motorTaskStart();
}