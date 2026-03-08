#include "driverlib.h"
#include "device.h"
#include "FreeRTOS.h"
#include "queue.h"


void can_task_init(void);

void can_task_loop(void*);

QueueHandle_t can_get_message_queue(void);

void can_get_isr_stats(uint32_t* messages, uint32_t* drops);

__interrupt void CANA0_ISR(void);