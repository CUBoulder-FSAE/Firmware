//VCU Logger
#include "logger.h"

#include "device.h"
#include "driverlib.h"

#include "c2000_freertos.h"
#include "queue.h"


static QueueHandle_t loggerQueue;
void init() {
    loggerQueue = xQueueCreate(10, sizeof(uint32_t));
}


void logger_send(){
    logger_data data;
    BaseType_t loggerStatusSend;
    while(1){
        loggerStatusSend = xQueueSend(loggerQueue, &data, pdMS_TO_TICKS(0));
        /*
        Recieve data, send to queue: loggerStatusSend = xQueueSend(loggerQueue, &data, pdMS_TO_TICKS(100));
        Send for ISR?
        */
        //vTaskDelay(1000);
        if (loggerStatusSend != pdPASS) {
            // Failed to send (queue was full for the entire 100ms)
        }
    }
    

}

void logger_receive(){
   logger_data receivedData;
   BaseType_t loggerStatusReceive;
   
    while (1){
        loggerStatusReceive = xQueueReceive(loggerQueue, &receivedData, portMAX_DELAY);
        /*
        Receive data from queue: loggerStatusReceive = xQueueReceive(loggerQueue, &receivedData, portMAX_DELAY);
        Receive from ISR?
        */
        if (loggerStatusReceive != pdPASS) {
            // Failed to send (queue was full for the entire 100ms)
        }
    }
}
