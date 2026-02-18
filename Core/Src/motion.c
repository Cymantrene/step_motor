#include "motion.h"
#include "estop.h"

/* Внутренняя функция: получить Axis* по ID */
static Axis* Motion_GetAxis(Motion *m, MotionAxisId id) {
	if (id == AXIS_1)
		return m->a1;
	if (id == AXIS_2)
		return m->a2;
	return 0;
}

void Motion_Init(Motion *m, Axis *axis1, Axis *axis2, Driver *drv1,
		Driver *drv2) {
	m->a1 = axis1;
	m->a2 = axis2;
	m->d1 = drv1;
	m->d2 = drv2;
}

uint8_t Motion_IsBusy(Motion *m, MotionAxisId id) {
	Axis *a = Motion_GetAxis(m, id);
	if (!a)
		return 0;

	/* active = 1 пока Axis выполняет движение */
	return (uint8_t) (a->active != 0);
}

MotionResult Motion_Move(Motion *m, MotionAxisId id, int32_t steps) {
	/* Если активна авария — не разрешаем движение */
	if (Estop_IsActive())
		return MOTION_ESTOP;

	Axis *a = Motion_GetAxis(m, id);
	if (!a)
		return MOTION_BAD_AXIS;

	/* Если ось занята — новую команду не принимаем */
	if (a->active)
		return MOTION_BUSY;

	/*
	 * Команда движения:
	 * Axis_MoveSteps() принимает full_steps (полные шаги), а внутри переводит в микрошаги
	 * и запускает профиль.
	 */
	Axis_MoveSteps(a, steps);

	return MOTION_OK;
}

void Motion_EStop(Motion *m) {
	/* Устанавливаем флаг аварии (на случай, если вызвали не из EXTI) */
	Estop_Trigger();

	/* Жёстко останавливаем обе оси */
	Axis_EmergencyStop(m->a1);
	Axis_EmergencyStop(m->a2);

	/* Отключаем драйверы */
	Driver_DisableAll();
}

void Motion_StopAll(Motion *m) {
	/*
	 * Пока реализуем как жёсткий стоп.
	 * Позже можем сделать “мягкий стоп”: поставить target_speed=MIN и дать профилю затормозить.
	 */
	Axis_EmergencyStop(m->a1);
	Axis_EmergencyStop(m->a2);
	Driver_DisableAll();
}
