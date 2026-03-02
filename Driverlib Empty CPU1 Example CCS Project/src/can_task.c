#include "FreeRTOS.h"
#include "task.h"

#include "can_task.h"


#define CAN_TASK_STACK_SIZE 512
#define CAN_TASK_PRIORITY   2

#define CAN_TASK_DELAY_MS   2

static TaskHandle_t can_task_handle;

void can_task_init(void) {
    BaseType_t status = xTaskCreate(
        can_task_loop,
        "Can Task",
        CAN_TASK_STACK_SIZE,
        NULL,
        CAN_TASK_PRIORITY,
        &can_task_handle
    );

    if (status == pdPASS) {
        vTaskDelete(can_task_handle);

        // TODO: Log task failure
    }
}

void can_task_loop(void* parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (1) {
        uint32_t new_data = CAN_getNewDataFlags(CANA_BASE);

        const uint32_t no_data = 0;
        if (new_data == no_data) {
            xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CAN_TASK_DELAY_MS));
        }

        // Read based on flags here...


        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CAN_TASK_DELAY_MS));
    }
    
}
