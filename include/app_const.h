#include <stdio.h>

const char *TAG = "GPIO_APP";
const uint8_t DUTY_75 = (uint8_t)(255 * 0.75f);

enum MotorDirection {
    MOTOR_STOP = 0,
    MOTOR_FORWARD = 1,
    MOTOR_BACKWARD = 2
};
