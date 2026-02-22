#include "device.h"
#include "driverlib.h"

#include "c2000_freertos.h"
#include "queue.h"

#include <string.h>

typedef struct logger_data{
    //vars
    uint8_t *data_t;
    int size;
}logger_data;
