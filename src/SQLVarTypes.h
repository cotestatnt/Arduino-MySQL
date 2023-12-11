#ifndef SQL_TYPES_H
#define SQL_TYPES_H

#include <Arduino.h>

uint32_t readFixedLengthInt(const uint8_t * packet, int offset, int size);
uint32_t readLenEncInt(const uint8_t * packet, int offset);
void store_int(uint8_t *buff, long value, int size);

void readLenEncString(char* pString, const uint8_t * packet, int offset);
void readLenEncString(std::string &pString, const uint8_t *packet, int offset);

#endif