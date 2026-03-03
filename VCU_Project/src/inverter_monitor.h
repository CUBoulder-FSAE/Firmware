#include "driverlib.h"
#include "device.h"

// create can message object that can read all inverter transmit values
// create task 
// create queue
void inverter_task_init(void);


// read from CAN message queue
// depending on ID of the message parse for important information
// set the static variables to keep track of important things (is motor spinning, rpm of motor, )
void inverter_task_loop(void*);