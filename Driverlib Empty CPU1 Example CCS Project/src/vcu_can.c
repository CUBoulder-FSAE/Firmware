#include "can.h"
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

// CAN Module Configuration
#define CAN_MODULE              CANA_BASE               // CAN module to use
#define CAN_BITRATE_KBPS        500    

