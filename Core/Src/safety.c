// ===================== safety.c =====================
#include "safety.h"
#include "estop.h"   // <<< ДОБАВИЛИ: чтобы вызывать Estop_Trigger()

void Safety_Init(Safety *s, GPIO_TypeDef *estop_port, uint16_t estop_pin,
		Driver *drv1, Driver *drv2) {
	s->estop_port = estop_port;
	s->estop_pin = estop_pin;

	s->drv1 = drv1;
	s->drv2 = drv2;

	s->state = SAFETY_OK;
}

void Safety_Update(Safety *s) {
	// E-STOP активен (кнопка нажата / цепь разорвана)
	if (HAL_GPIO_ReadPin(s->estop_port, s->estop_pin) == GPIO_PIN_RESET) {
		if (s->state != SAFETY_ESTOP) {
			Estop_Trigger();                 // <<< ДОБАВИЛИ: общий флаг аварии

			Driver_EmergencyStop(s->drv1); // оставляем как резерв/доп. безопасность
			Driver_EmergencyStop(s->drv2);

			s->state = SAFETY_ESTOP;
		}
	}
}

SafetyState Safety_GetState(Safety *s) {
	return s->state;
}
