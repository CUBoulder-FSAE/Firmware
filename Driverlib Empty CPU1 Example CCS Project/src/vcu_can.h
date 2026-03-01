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

void CAN_initModule(void);
void CAN_sendMessage(uint32_t msgId, uint8_t *data, uint8_t dlc);
uint8_t CAN_receiveMessage(CAN_Message_t *msg);

#endif