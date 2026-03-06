#include "inverter_transmit.h"
#include "device.h"

#define NODE_ID 4

#define SEND_CURRENT_OBJ_ID 1
#define DRIVE_EN_OBJ_ID 2

#define SEND_CURRENT_MSG_ID ((0x01 << 6) | NODE_ID)
#define DRIVE_EN_MSG_ID ((0x0C << 6) | NODE_ID)

#define MAX_CURRENT 200
#define MIN_CURRENT 0

// create message objects (drive enable and set current)
void inverter_transmit_init()
{
    CAN_setupMessageObject
    (
        CANA_BASE,
        SEND_CURRENT_OBJ_ID,
        SEND_CURRENT_MSG_ID,
        CAN_MSG_FRAME_STD,
        CAN_MSG_OBJ_TYPE_TX,
        0U,  // No ID masking
        CAN_MSG_OBJ_NO_FLAGS,
        8U   // Data length
    ); 

    CAN_setupMessageObject
    (
        CANA_BASE,
        DRIVE_EN_OBJ_ID,
        DRIVE_EN_OBJ_ID,
        CAN_MSG_FRAME_STD,
        CAN_MSG_OBJ_TYPE_TX,
        0U,  // No ID masking
        CAN_MSG_OBJ_NO_FLAGS,
        8U   // Data length
    ); 
}




void inverter_transmit_set_current(uint8_t current) 
{
    if (current > MAX_CURRENT || current < MIN_CURRENT) return;

    CAN_sendMessage(CANA_BASE, SEND_CURRENT_OBJ_ID, 1, &current);
}

void inverter_transmit_drive_enable()
{
    uint8_t enabled = 1;
    CAN_sendMessage(CANA_BASE, DRIVE_EN_OBJ_ID, 1, &enabled);
}

