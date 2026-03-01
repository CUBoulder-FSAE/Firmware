#include "device.h"
#include "driverlib.h"

#include "c2000_freertos.h"
#include "queue.h"

#include <string.h>

#define LOGGER_QUEUE_SIZE 10u
#define LOGGER_WAIT_TO_SEND 100u

typedef struct logger_data{
    //vars
    uint8_t *data_t;
    int size;
} logger_data;

//Initializes queue
void init();

void logger_send(logger_data);

void logger_receive(logger_data);
