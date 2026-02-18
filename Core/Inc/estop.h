#ifndef ESTOP_H
#define ESTOP_H

#include <stdint.h>

/* Срабатывание аварии (вызываем из EXTI callback) */
void Estop_Trigger(void);

/* Проверка: активна ли авария */
uint8_t Estop_IsActive(void);

/* Сброс аварии (опционально, можно не использовать) */
void Estop_Clear(void);

#endif /* ESTOP_H */
