//VCU Logger
#include "logger.h"

static QueueHandle_t loggerQueue;
void init() {
    loggerQueue = xQueueCreate(LOGGER_QUEUE_SIZE, sizeof(uint32_t));
}

void logger_send(logger_data sendData){
    //logger_data data;
    BaseType_t loggerStatusSend;
    while(1){
        loggerStatusSend = xQueueSend(loggerQueue, &sendData.data_t, pdMS_TO_TICKS(LOGGER_WAIT_TO_SEND));
        //Send to queue, wait for max of 100ms if queue is full
        
        //vTaskDelay(1000);
        if (loggerStatusSend != pdPASS) {
            // Failed to send (queue was full for the entire 100ms)
            // #error Failed to send to queue
        }
    }
    

}

void logger_receive(logger_data receivedData){
   //logger_data receivedData;
   BaseType_t loggerStatusReceive;
   
    while (1){
        loggerStatusReceive = xQueueReceive(loggerQueue, &receivedData.data_t, portMAX_DELAY);
        //Receive data from queue, wait indefinitely until data is available
        
        if (loggerStatusReceive != pdPASS) {
            //#error Failed to Receive to queue
        }
    }
}
