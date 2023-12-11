#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include "SQLVarTypes.h"

typedef enum
{
    PACKET_OK = 0x00,
    PACKET_UNKNOWN = 0x01,
    PACKET_TEXTRESULTSET = 0x02,
    PACKET_EOF = 0xFE,
    PACKET_ERR = 0xFF
} Packet_Type;


// /**
//  * @brief Stores the raw MySQL packet
//  * 
//  * @param payload_length Size of the payload in bytes.
//  * @param sequence_id MySQL packet number in current dialog.
//  * @param payload MySQL query result of size `payload_length`.
//  */
// typedef struct {
//     uint8_t payload_length[3];
//     uint8_t sequence_id;
//     uint8_t *payload;
// } mysql_packet_t;



class MySQL_Packet
{
public:
    MySQL_Packet(const uint8_t *pPacket)
    {
        this->mPayloadLength = readFixedLengthInt(pPacket, 0, 3);
        this->mPacketNumber = readFixedLengthInt(pPacket, 3, 1);
        this->mPayload = (uint8_t *)malloc(sizeof(uint8_t) * this->mPayloadLength);
        memcpy(this->mPayload, pPacket + 4, this->mPayloadLength);
    }

    MySQL_Packet()
    {
        this->mPayloadLength = 0;
        this->mPacketNumber = 0;
        this->mPayload = NULL;
    }

    ~MySQL_Packet()
    {
        free(this->mPayload);
        this->mPayload = NULL;
        this->mPayloadLength = 0;
        this->mPacketNumber = 0;
    }

    Packet_Type getPacketType(void);

    uint32_t getPacketLength(void)
    {
        return this->mPayloadLength + 4;
    }

    uint32_t mPacketNumber = 0;
    uint32_t mPayloadLength = 0;
    uint8_t *mPayload = NULL;
};

#endif