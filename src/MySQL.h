/**
 * @file MySQL.h
 * @author Cotesta Tolentino (cotestatnt@yahoo.com)
 * @brief ESP32-MySQL library using Arduino framework
 * @version 2.0
 * @date 09/12/2023
 */

#ifndef MYSQL_H
#define MYSQL_H

#include <iostream>
#include <vector>
#include <string>
#include <Arduino.h>
#include <WiFi.h>

#include "SHA1.h"
#include "PacketsTypes.h"
#include "SQLVarTypes.h"
#include "DataQuery.h"

#define DEBUG 0
#define MAX_PRINT_LEN 32

const char CONNECTED[] PROGMEM = "Connected to server version ";
const char DISCONNECTED[] PROGMEM = "Disconnected.";

/**
 * @brief Used to send data over TCP socket.
 */
#define  BUFF_SIZE (2 * CONFIG_LWIP_TCP_MSS)


class MySQL
{
public:
    /**
     * @brief Creates a MySQL object
     *
     * @param pTCPSocket Attached socket to a network interface
     * @param server_ip MySQL server IP address
     */
    MySQL(Client *pClient, const char *server_ip, uint16_t);
    /**
     * @brief Destroys the MySQL object
     *
     */
    ~MySQL(void);
    /**
     * @brief Connect to MySQL under specific session
     *
     * @param user Username
     * @param password Password
     * @return true Connection established and session opened
     * @return false Unable to connect or login
     */
    bool connect(const char *user, const char *password, const char* db = nullptr);
    /**
     * @brief Check is client is connected to MySQL server
     *
     * @return true connected
     * @return false disconnected
     */
    bool connected();
    /**
     * @brief Disconnects from MySQL server by closing session
     *
     * @return true OK
     * @return false Unable to send disconnect command to server
     */
    bool disconnect();
    /**
     * @brief Send a simple query and expect Table as result
     * @param Database Database structure to store results
     * @param pQuery Query
     * @return bool state
     */
    bool query(DataQuery_t & database, const char *pQuery);
    /**
     * @brief Prints the recieved table to the default stdout buffer
     * @param Database Database structure to store results
     *
     */
    void printHeading(std::vector<Field_t> &fields);
    void printResult(DataQuery_t & database);

private:
    // User-configured TCP socket attached to NetworkInterface
    Client *client = NULL;

    char* server_version = NULL;

    // Class containing packet header and payload
    MySQL_Packet *packet = NULL;

    // Fixed-Size buffer for raw data from TCP socket
    uint8_t tcp_socket_buffer[BUFF_SIZE] = {0};

    // MySQL Server IP
    const char *mServerIP = NULL;
    uint16_t mPort = 3306;

    // MySQL packets parsed from mBuffer
    std::vector<MySQL_Packet*> mPacketsRecieved;

    // Seed used to hash password through SHA-1
    uint8_t mSeed[20] = {0};

    bool recieve(void);
    uint16_t write(char *message, uint16_t len);
    int send_authentication_packet(const char *user, const char *password, const char *db);
    void parse_handshake_packet(void);
    bool parse_textresultset(DataQuery_t* dataquery);
    void free_recieved_packets(void);
    int  scramble_password(const char *password, uint8_t *pwd_hash);
    void flush_packet(void);
    void parse_error_packet(const MySQL_Packet *packet, uint16_t packet_len);

#if DEBUG
    void print_packets_types(void);

    void printRawBytes(const uint8_t* data, size_t len) {
        Serial.printf("Packet length: %d\n", len);
        for(int i =0; i<len; i++) {
            Serial.printf( "%02X ", data[i]);
            if ((i+1) % 50 == 0) Serial.println();
        }
        Serial.println("\n");
    }
#endif
};

#endif