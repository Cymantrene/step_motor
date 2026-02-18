#ifndef DRIVER_H
#define DRIVER_H


#include "stm32g4xx_hal.h"
#include <stdint.h>


// состояние драйвера
typedef enum
{
DRIVER_DISABLED = 0,
DRIVER_ENABLED = 1
} DriverState;


typedef struct
{
GPIO_TypeDef *enable_port;
uint16_t enable_pin;


uint8_t enable_active_level; // 1 = ENABLE по HIGH, 0 = ENABLE по LOW
DriverState state;


} Driver;


void Driver_Init(Driver *d,
GPIO_TypeDef *enable_port,
uint16_t enable_pin,
uint8_t enable_active_level);


void Driver_Enable(Driver *d);
void Driver_Disable(Driver *d);
void Driver_EmergencyStop(Driver *d);

typedef enum
{
  DRV_MS_1   = 1,
  DRV_MS_2   = 2,
  DRV_MS_4   = 4,
  DRV_MS_8   = 8,
  DRV_MS_16  = 16,
  DRV_MS_32  = 32,
  DRV_MS_64  = 64
} DriverMicrostepMode;

void Driver_SetMicrostep1(DriverMicrostepMode mode);
void Driver_SetMicrostep2(DriverMicrostepMode mode);
void Driver_DisableAll(void);

#endif
