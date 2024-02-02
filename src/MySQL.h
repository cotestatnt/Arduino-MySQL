/**
 * @file MySQL.h
 * @author Cotesta Tolentino (cotestatnt@yahoo.com)
 * @brief Arduino MySQL client library
 * @version 1.0.1
 * @date 09/12/2023
 */

#ifndef MYSQL_H
#define MYSQL_H

#include <stdarg.h>
#include <Arduino.h>
#include <Client.h>

#include "SHA1.h"
#include "PacketsTypes.h"
#include "SQLVarTypes.h"
#include "DataQuery.h"


#if defined(__AVR__)
 #include <ArduinoSTL.h>
#else
#include <cstdint>
#include <vector>
#endif


#define DEBUG 0
#define MAX_PRINT_LEN 32

const char CONNECTED[] PROGMEM = "Connected to MySQL server version ";
const char DISCONNECTED[] PROGMEM = "Disconnected.";

/**
 * @brief Used to send data over TCP socket.
 */
#if defined(ESP32)
#define  BUFF_SIZE (2 * CONFIG_LWIP_TCP_MSS)
#else
#define  BUFF_SIZE (1024)
#endif


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
    template <typename TDestination>
    void printHeading(std::vector<Field_t> &fields, TDestination& destination) {
        char sep[MAX_PRINT_LEN + 3] = { 0 };
        const int printfLen = MAX_PRINT_LEN + 4;
        int str_len;

        // Print a row separator
        for (Field_t field : fields) {
            memset(sep, 0, MAX_PRINT_LEN);
            str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
            this->printf_n(destination, printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
        }
        destination.print("+\n");

        // Print fields name
        for (Field_t field : fields) {
            memset(sep, 0, MAX_PRINT_LEN);
            str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
            this->printf_n(destination, printfLen, "| %*s ", str_len, field.name.c_str());
        }
        destination.print("|\n");

        // Print a row separator again
        for (Field_t field : fields) {
            memset(sep, 0, MAX_PRINT_LEN);
            str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
            this->printf_n(destination, printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
        }
        destination.print("+\n");
    }


    template <typename TDestination>
    void printResult(DataQuery_t & database, TDestination& destination)
    {
        printHeading(database.fields, destination);
        int printfLen = MAX_PRINT_LEN + 4;
        int str_len;

        // Print records values
        for (Record_t rec : database.records) {
            int i = 0;
            for (String value: rec.record) {
                str_len = (database.fields.at(i).size > MAX_PRINT_LEN || database.fields.at(i).size == 0)
                    ? MAX_PRINT_LEN : database.fields.at(i).size;

                if (!value.length())
                    value = " ";
                if (value.length() > MAX_PRINT_LEN) {
                    value = value.substring(0, MAX_PRINT_LEN);
                    value.replace(value.substring(MAX_PRINT_LEN-3, MAX_PRINT_LEN), "...");
                }
                this->printf_n(destination, printfLen + 1, "| %*s ", str_len, value.c_str());
                i++;
            }
            destination.print("|\n");
        }

        // Print last row separator
        for (Field_t field : database.fields) {
            char sep[MAX_PRINT_LEN+3] = { 0 };
            str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN : field.size;
            this->printf_n(destination, printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
        }
        destination.print("+\n");
    }

    const char* getLastSQLSTATE() {
        return SQL_state;
    }

    const char* getLastError() {
        return error_message.c_str();
    }

private:
    // User-configured TCP socket attached to NetworkInterface
    Client *client = nullptr;

    char* server_version = nullptr;

    // Store last SQL state (usefull for error handling)
    char SQL_state[6];
    String error_message;

    // Class containing packet header and payload
    MySQL_Packet *packet = nullptr;

    // Fixed-Size buffer for raw data from TCP socket
    uint8_t tcp_socket_buffer[BUFF_SIZE] = {0};

    // MySQL Server IP
    const char *mServerIP = nullptr;
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

    // Variadic function that will execute the query selected with passed parameters
    template <typename TDestination>
    void printf_n(TDestination& destination, size_t n, const char* fmt, ...) {
        char buf[n];
        va_list args;
        va_start (args, fmt);
        vsnprintf (buf, sizeof(buf), fmt, args);
        va_end (args);
        destination.print(buf);
    }

#if DEBUG
    void print_packets_types(void);

    void printRawBytes(const uint8_t* data, size_t len) {
        char buf[32];
        snprintf(buf, 32, "Packet length: %d\n", len);
        Serial.print(buf);
        for(int i =0; i<len; i++) {
            snprintf(buf, 32, "%02X ", data[i]);
            Serial.print(buf);
            if ((i+1) % 50 == 0) Serial.println();
        }
        Serial.println("\n");
    }
#endif
};

#endif