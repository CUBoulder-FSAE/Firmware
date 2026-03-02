//VCU Logger
#include "logger.h"

static QueueHandle_t loggerQueue;
void init() {
    loggerQueue = xQueueCreate(LOGGER_QUEUE_SIZE, sizeof(uint32_t));
}

void logger_send(logger_data sendData){
    logger_data data;
    //memcpy(sendData.data, data, data_len)
    memcpy(&sendData, &data, sendData.data_size);
    BaseType_t loggerStatusSend;

    loggerStatusSend = xQueueSend(loggerQueue, &sendData, pdMS_TO_TICKS(LOGGER_WAIT_TO_SEND));
    //Send to queue, wait for max of 100ms if queue is full
    if(loggerStatusSend != pdPASS){
        // Failed to send (queue was full for the entire 100ms)
        return;
    }

}

void logger_receive(logger_data receivedData){
   //logger_data receivedData;
   BaseType_t loggerStatusReceive;
   
    while (1){
        loggerStatusReceive = xQueueReceive(loggerQueue, &receivedData, portMAX_DELAY);
        //Receive data from queue, wait indefinitely until data is available
        
        if (loggerStatusReceive != pdPASS) {
            //log fail
            return;
        }
        //send to CAN
        CAN_Message_t msg;
        //takes define
        msg.id = LOGGER_CAN_ID;
        //data
        msg.data = receivedData.data_t;
        //data length
        msg.dlc = receivedData.data_size;

        //CAN_send
    }
}
//vcu_can_send_message
