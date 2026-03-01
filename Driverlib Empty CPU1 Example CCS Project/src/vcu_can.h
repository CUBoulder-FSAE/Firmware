//
// FILE: can_communication.h
//
// DESCRIPTION: CAN communication driver header
//              Public API for CAN transmit, receive, and unit testing
//

#ifndef VCU_CAN_H
#define VCU_CAN_H

#include <stdint.h>
#include <stdbool.h>

//
// CAN Message Structure
//
typedef struct {
    uint32_t id;                // Message ID (11-bit standard)
    uint8_t dlc;                // Data Length Code (0-8)
    uint8_t* data;            // Message data bytes
    uint32_t timestamp;         // Timestamp of message
} CAN_Message_t;

void vcu_can_init(void);
void vcu_can_send_message(CAN_Message_t);
uint8_t vcu_can_receive_message(CAN_Message_t *msg);

#endif