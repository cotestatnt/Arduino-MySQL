#include "MySQL.h"



/**
 * @brief Creates a MySQL o
 *
 * @param pTCPSocket Attached socket to a network interface
 * @param server_ip MySQL server IP address
 */
MySQL::MySQL(Client *pClient, const char *server_ip, uint16_t port) : mServerIP(server_ip)
{
    mPort = port;
    client = pClient;
    packet  = new MySQL_Packet();
}

/**
 * @brief Destroys the MySQL object
 *
 */
MySQL::~MySQL(void)
{
    //Close MySQL Session
    this->disconnect();
    this->free_recieved_packets();
}

/**
 * @brief Connect to MySQL under specific session
 *
 * @param user Username
 * @param password Password
 * @return true Connection established and session opened
 * @return false Unable to connect or login
 */
bool MySQL::connect(const char *user, const char *password, const char* db)
{
    if (client == nullptr)
        return false;
    bool connected = false;

    //Set MySQL server IP
    IPAddress server;
    server.fromString(mServerIP);
    int retries = 5;
    // Retry up to MAX_CONNECT_ATTEMPTS times.
    while (retries--) {
        connected = client->connect(server, 3306);
        if (connected ) {
            break;
        }
        delay(100);
    }

    if (!connected )
        return false;

    //Set socket Timeout
    client->setTimeout(1000);

    //Read hadshake packet
    flush_packet();

    //Parse packet
    parse_handshake_packet();

    //Send authentification to server
    connected = (send_authentication_packet(user, password, db) > 0) ? true : false;

    Serial.print(CONNECTED);
    Serial.print(server_version);
    Serial.print("\n");

    return connected;
}

/**
 * @brief Disconnects from MySQL server by closing session
 *
 * @return true OK
 * @return false Unable to send disconnect command to server
 */
bool MySQL::disconnect()
{
    uint8_t COM_QUIT[] = {0x01, 0x00, 0x00, 0x00, 0x01};

    //Send COM_QUIT packet (Payload : 0x01)
    if (this->write((char *)COM_QUIT, 5) > 0)
        return true;

    return false;
}

/**
 * @brief Check is client is connected to MySQL server
 *
 * @return true connected
 * @return false disconnected
 */
bool MySQL::connected() {
    return client->connected();
}

/**
 * @brief Recieve MySQL packet over TCP socket
 *
 * @return true recieved and stores MySQL packet
 * @return false nothing to read or MySQL packet corrupted
 */
bool MySQL::recieve(void) {

    // Number of bytes recieved over TCP socket
    int recv_len = 0;

    // Setup TCP Socket
    this->client->setTimeout(5000);
    /**
     * Recieve packet header.
     *
     * We MUST recieve 4 bytes :
     * - The payload length (encoded int<3>)
     * - The sequence ID    (encoded int<1>)
     */
    recv_len = this->client->readBytes(tcp_socket_buffer, 4);

    if (recv_len == 4) {
        // Class containing packet header and payload
        MySQL_Packet *packet  = new MySQL_Packet();

        // First 3 bytes are the payload lenth
        packet->mPayloadLength = readFixedLengthInt(tcp_socket_buffer, 0, 3);

        // Fourth byte is the sequence ID
        packet->mPacketNumber = readFixedLengthInt(tcp_socket_buffer, 3, 1);

        // Serial.printf("Packet #%d, length %d bytes\n", packet->mPacketNumber, packet->mPayloadLength);

        if (packet->mPayloadLength <= BUFF_SIZE) {
            memset(tcp_socket_buffer, 0, BUFF_SIZE);

            /**
             * The following bytes are the actual
             * payload, we must match the payload
             * size once we recieved the 4 bytes.
             */
            recv_len = this->client->readBytes(tcp_socket_buffer, packet->mPayloadLength);

            if (recv_len == (int)(packet->mPayloadLength)) {
                packet->mPayload = (uint8_t *)calloc((size_t)(packet->mPayloadLength), sizeof(uint8_t));
                memcpy(packet->mPayload, tcp_socket_buffer, (size_t)(packet->mPayloadLength));
                this->mPacketsRecieved.push_back(packet);
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Send bytes over TCP socket to MySQL server
 *
 * @param message
 * @param len
 * @return int
 */
uint16_t MySQL::write(char *message, uint16_t len) {
    //Send raw data to socket
    return client->write((const uint8_t *)message, len);
}

/**
 * @brief Send a simple query and expect Table as result
 * @param Database Database structure to store results
 * @param pQuery Query
 * @return bool state
 */
bool MySQL::query(DataQuery_t & dataquery, const char *pQuery) {

    uint16_t tcp_socket_write_size = 0;
    memset(tcp_socket_buffer, 0, BUFF_SIZE);

    // Free recieved packets
    this->free_recieved_packets();

    // packet_len without header
    int payload_len = strlen(pQuery) + 1;

    // payload_len + 4
    int packet_len = payload_len + 4;

    // Check if query is sendable over TCP socket by checking final size
    if (packet_len <= BUFF_SIZE) {
        /**
         * Now that we are sure user did not sent
         * a query too big for TCP socket we can
         * prepare the query packet before sending
         * it.
         *
         * Packet layout :
         * int<3>	    payload_length
         * int<1>	    sequence_id
         * string<var>	payload
         *
         * Source : https://dev.mysql.com/doc/internals/en/mysql-packet.html
         */

        // Payload length
        store_int((uint8_t *)tcp_socket_buffer, payload_len, 3);

        // Sequence ID to 0 : initiator
        tcp_socket_buffer[3] = 0;

        // Query type to 0x03 (COM_QUERY)
        tcp_socket_buffer[4] = 0x03;

        // Copy request into payload skipping first byte which is the COM type
        memcpy(tcp_socket_buffer + 5, pQuery, payload_len - 1);

        // Write data over TCP socket
        tcp_socket_write_size = this->write((char *)tcp_socket_buffer, packet_len);

        // Check if TCP socket managed to send packet
        if (tcp_socket_write_size == packet_len) {
            /**
             * Query completely sent over TCP
             * socket to server, we now have to :
             * - Recieve server response
             * - Parse server response into packets
             * - Notify user on com status
             *
             * After a query, there is multiple
             * server response possible :
             * - ERR : Error packet containing a string
             * - OK : Query completed without expecting further information/data
             * - Table response
             *      - Field count (length encoded int)
             *      - [n] Fields
             *      - EOF
             *      - [?] ROW (Until following EOF or ERR)
             *      - EOF or ERR
             *
             * Source : https://dev.mysql.com/doc/internals/en/com-query-response.html
             */

            // Used to track recieve function return
            bool tcp_packet_status = false;

            // MySQL packet type
            Packet_Type type = PACKET_UNKNOWN;

            // Recieve one packet
            tcp_packet_status = this->recieve();

            if (tcp_packet_status) {
                /**
                 * Packet has been recieved entirely.
                 * we must check if first byte of
                 * payload is 0xFE.
                 * In that case it may either be :
                 * - Length encoded integer (Payload > 8 bytes)
                 * - EOF packet
                 */
                type = this->mPacketsRecieved.at(0)->getPacketType();

                if (type == PACKET_TEXTRESULTSET) {
                    /**
                     * We must follow the TextResultSet pattern
                     * Source : https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-ProtocolText::Resultset
                     */

                    // Fields
                    while (tcp_packet_status && (type != PACKET_EOF)) {
                        // Recieve one MySQL packet over TCP socket
                        tcp_packet_status = this->recieve();

                        if (tcp_packet_status) {
                            // Get the last packet type
                            type = this->mPacketsRecieved.at(this->mPacketsRecieved.size() - 1)->getPacketType();
                        }
                    }

                    // Reset type to loop for rows
                    type = PACKET_UNKNOWN;

                    // Rows
                    while (tcp_packet_status && (type != PACKET_EOF) && (type != PACKET_ERR)) {
                        // Recieve one MySQL packet over TCP socket
                        tcp_packet_status = this->recieve();

                        if (tcp_packet_status) {
                            // Get the last packet type
                            type = this->mPacketsRecieved.at(this->mPacketsRecieved.size() - 1)->getPacketType();
                        }
                    }

                    if (tcp_packet_status) {
                        return this->parse_textresultset(&dataquery);
                    }
                    this->free_recieved_packets();
                    return false;
                }
                else if (type == PACKET_ERR) {
                    this->parse_error_packet(
                        this->mPacketsRecieved.at(this->mPacketsRecieved.size() - 1),
                        this->mPacketsRecieved.at(this->mPacketsRecieved.size() - 1)->getPacketLength()
                    );
                    this->free_recieved_packets();
                    return false;
                }
                this->free_recieved_packets();
                return true;
            }
        }
    }
    this->free_recieved_packets();
    return false;
}

/**
 * @brief Free packets vector content and empties it
 *
 */
void MySQL::free_recieved_packets(void)
{
    if (this->mPacketsRecieved.size() > 0) {
        for (size_t i = 0; i < this->mPacketsRecieved.size(); i++)  {
            delete this->mPacketsRecieved.at(i);
        }
        this->mPacketsRecieved.clear();
    }
}


/**
 * @brief Parses recieved packets considered as a table result
 *
 * @return true Table parsed
 * @return false Unable to parse table
 */
bool MySQL::parse_textresultset(DataQuery_t* database)
{
    // Used to keep track of the actual packet
    int packet_offset = 0;
    Packet_Type packet_type = PACKET_OK;
    const uint8_t *packet = nullptr;

    packet = this->mPacketsRecieved.at(packet_offset)->mPayload;
    packet_type = this->mPacketsRecieved.at(packet_offset)->getPacketType();

    //Store the column count into the table structure
    database->fieldCount = readFixedLengthInt(packet, 0, 1);

    for (int i = 0; packet_type != PACKET_EOF; i++) {
        //Check the next packet type to exit the for loop if needed
        packet_offset++;
        packet = this->mPacketsRecieved.at(packet_offset)->mPayload;
        packet_type = this->mPacketsRecieved.at(packet_offset)->getPacketType();
        #if DEBUG
            printRawBytes(packet, this->mPacketsRecieved.at(packet_offset)->getPacketLength());
        #endif

        if (packet_type == PACKET_EOF)
            break;

        int offset = 0;
        int str_len = readLenEncInt(packet, offset);    // def
        offset += str_len + 1;

        // Skip database name and table name (we know in advance, no need to parse it)
        for (int i =0; i<3; i++) {
            offset += readLenEncInt(packet, offset) + 1 ;
        }

        // Allocate enougth memory and get field name (this can be an alias)
        str_len = readLenEncInt(packet, offset);
        char * field_name = (char*)malloc((str_len + 1) * sizeof(char));
        offset += 1 + readLenEncString(field_name, packet, offset);

        // Create new Field_t field in order to add to std::vector
        Field_t field;
        field.name = field_name;
        #if DEBUG
            this->printf_n(64, "next offset %02X, field %s\n", offset, field.name.c_str());
        #endif

        //  Reallocate enougth memory and get the real name of field (NO alias)
        str_len = readLenEncInt(packet, offset);
        field_name = (char*)realloc(field_name, (str_len + 1) * sizeof(char));
        offset += 1 + readLenEncString(field_name, packet, offset);

        // Offset for field size (field name length + 3)
        field.size = readFixedLengthInt(packet, offset + 3, 4);

        free(field_name);    // Free memory
        database->fields.push_back(field);
    }

    packet_offset++;
    packet = this->mPacketsRecieved.at(packet_offset)->mPayload;
    packet_type = this->mPacketsRecieved.at(packet_offset)->getPacketType();
    #if DEBUG
        printRawBytes(packet, this->mPacketsRecieved.at(packet_offset)->getPacketLength());
    #endif

    // Row parsing : if the recieved packet is not an EOF or ERR packet
    if (packet_type == PACKET_TEXTRESULTSET) {

        for (int row = 0; packet_type != PACKET_EOF; row++) {
            int str_offset = 0;

            // Increment number of rows
            database->recordCount = row + 1;

            // Get row values
            Record_t newRecord;
            for (int col = 0; col < database->fieldCount; col++) {
                int str_size = readLenEncInt(packet, str_offset);               // Get string length
                char * value = (char*)malloc((str_size + 2) * sizeof(char));    // Allocate enougth memory
                str_offset += 1 + readLenEncString(value, packet, str_offset);  // Get te text
                #if DEBUG
                    this->printf_n(128, "Field value: %s, length %d\n", str_offset, value, str_size);
                #endif
                newRecord.record.push_back(value);
                free(value);    // Free memory
            }
            database->records.push_back(newRecord);

            // Increment offset
            packet_offset++;
            packet = this->mPacketsRecieved.at(packet_offset)->mPayload;
            packet_type = this->mPacketsRecieved.at(packet_offset)->getPacketType();
            #if DEBUG
                printRawBytes(packet, this->mPacketsRecieved.at(packet_offset)->getPacketLength());
            #endif
        }


        this->free_recieved_packets();
        return true;
    }
    this->free_recieved_packets();
    return false;
}

/*
  parse_error_packet - Display the error returned from the server

  This method parses an error packet from the server and displays the
  error code and text via Serial.print. The error packet is defined
  as follows.

  Bytes                       Name
  -----                       ----
  1                           field_count, always = 0xff
  2                           errno
  1                           (sqlstate marker), always '#'
  5                           sqlstate (5 characters)
  n                           message
*/
void MySQL::parse_error_packet(const MySQL_Packet *packet, uint16_t packet_len )
{

    Serial.print("************** SQL Error *****************\n");
    memcpy(SQL_state, packet->mPayload + 4, 5);
    SQL_state[5] = '\0';
    Serial.print("SQLSTATE = ");
    Serial.print(SQL_state);
    Serial.print("\n");
    error_message = "";
    for (int i = 9; i < packet_len-4; i++) {
        char ch = (char)packet->mPayload[i];
        if (ch != '\0') {
            Serial.print(ch);
            error_message += ch;
        }
         else
            continue;
    }
    Serial.print("\n******************************************\n\n");
    // printRawBytes(packet->mPayload, packet_len);
}

/**
 * @brief Prints the recieved table to the default stdout buffer
 *
 */


// void MySQL::printHeading(std::vector<Field_t> &fields) {
//     char sep[MAX_PRINT_LEN+3] = { 0 };
//     char buf[fields.size() * (MAX_PRINT_LEN+3)];

//     // Print a row separator
//     for (Field_t field : fields) {
//         memset(sep, 0, MAX_PRINT_LEN);
//         int len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
//         sprintf(buf, "+%s", (char*)memset(sep, '-', len +2));
//         Serial.print(buf);
//     }
//     Serial.print("+\n");

//     // Print fields name
//     for (Field_t field : fields) {
//         memset(sep, 0, MAX_PRINT_LEN);
//         int len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
//         sprintf(buf,"| %*s ", len, field.name.c_str());
//         Serial.print(buf);
//     }
//     Serial.print("|\n");

//     // Print a row separator again
//     for (Field_t field : fields) {
//         memset(sep, 0, MAX_PRINT_LEN);
//         int len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
//         sprintf(buf, "+%s", (char*)memset(sep, '-', len +2));
//         Serial.print(buf);
//     }
//     Serial.print("+\n");
// }



// void MySQL::printResult(DataQuery_t & database)
// {
//     printHeading(database.fields);
//     char buf[database.fields.size() * (MAX_PRINT_LEN+3)];

//     // Print records values
//     for (Record_t rec : database.records) {
//         int i = 0;
//         for (String value: rec.record) {
//             int len = (database.fields.at(i).size > MAX_PRINT_LEN || database.fields.at(i).size == 0)
//                 ? MAX_PRINT_LEN : database.fields.at(i).size;

//             if (!value.length()) value = " ";
//             if (value.length() > MAX_PRINT_LEN) {
//                 value = value.substring(0, MAX_PRINT_LEN);
//                 value.replace(value.substring(MAX_PRINT_LEN-3, MAX_PRINT_LEN), "...");
//             }
//             sprintf(buf,"| %*s ", len, value.c_str());
//             Serial.print(buf);
//             i++;
//         }
//         Serial.print("|\n");
//     }

//     // Print last row separator
//     for (Field_t field : database.fields) {
//         char sep[MAX_PRINT_LEN+3] = { 0 };
//         int len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN : field.size;
//         sprintf(buf, "+%s", (char*)memset(sep, '-', len +2));
//         Serial.print(buf);
//     }
//     Serial.print("+\n");
// }


void MySQL::printHeading(std::vector<Field_t> &fields) {
    char sep[MAX_PRINT_LEN + 3] = { 0 };
    const int printfLen = MAX_PRINT_LEN + 4;
    int str_len;

    // Print a row separator
    for (Field_t field : fields) {
        memset(sep, 0, MAX_PRINT_LEN);
        str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
        this->printf_n(printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
    }
    Serial.print("+\n");

    // Print fields name
    for (Field_t field : fields) {
        memset(sep, 0, MAX_PRINT_LEN);
        str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
        this->printf_n(printfLen, "| %*s ", str_len, field.name.c_str());
    }
    Serial.print("|\n");

    // Print a row separator again
    for (Field_t field : fields) {
        memset(sep, 0, MAX_PRINT_LEN);
        str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN :  field.size;
        this->printf_n(printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
    }
    Serial.print("+\n");
}



void MySQL::printResult(DataQuery_t & database)
{
    printHeading(database.fields);
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
            this->printf_n(printfLen + 1, "| %*s ", str_len, value.c_str());
            i++;
        }
        Serial.print("|\n");
    }

    // Print last row separator
    for (Field_t field : database.fields) {
        char sep[MAX_PRINT_LEN+3] = { 0 };
        str_len = (field.size > MAX_PRINT_LEN || field.size == 0) ? MAX_PRINT_LEN : field.size;
        this->printf_n(printfLen, "+%s", (char*)memset(sep, '-', str_len +2));
    }
    Serial.print("+\n");
}

/**
 * @brief Hash password using server seed
 *
 * @param password MySQL session password
 * @param pwd_hash server seed
 * @return int (0 : Error | 1 : OK)
 */
int MySQL::scramble_password(const char *password, uint8_t *pwd_hash)
{
    SHA1Context sha;
    int word = 0, shift = 24, count = 3;
    uint8_t hash1[20];
    uint8_t hash2[20];
    uint8_t hash3[20];
    uint8_t pwd_buffer[40];

    if (strlen(password) == 0)
        return 0;

    // hash1
    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *)password, strlen(password));
    SHA1Result(&sha);
    for (int i = 0; i < 20; i++) {
        hash1[i] = (sha.Message_Digest[word] >> shift);
        shift = shift - 8;
        if (i == count) {
            shift = 24;
            word++;
            count += 4;
        }
    }
    word = 0;
    shift = 24;
    count = 3;

    // hash2
    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *)hash1, 20);
    SHA1Result(&sha);
    for (int i = 0; i < 20; i++) {
        hash2[i] = (sha.Message_Digest[word] >> shift);
        shift = shift - 8;
        if (i == count) {
            shift = 24;
            word++;
            count += 4;
        }
    }
    word = 0;
    shift = 24;
    count = 3;

    // hash3 of seed + hash2
    SHA1Reset(&sha);
    memcpy(pwd_buffer, &mSeed, 20);
    memcpy(pwd_buffer + 20, hash2, 20);
    SHA1Input(&sha, (const unsigned char *)pwd_buffer, 40);
    SHA1Result(&sha);
    for (int i = 0; i < 20; i++) {
        hash3[i] = (sha.Message_Digest[word] >> shift);
        shift = shift - 8;
        if (i == count) {
            shift = 24;
            word++;
            count += 4;
        }
    }
    word = 0;
    shift = 24;
    count = 3;

    // XOR for hash4
    for (int i = 0; i < 20; i++)
        pwd_hash[i] = hash1[i] ^ hash3[i];

    return 1;
}



/**
 * @brief Responds to the server handshake sequence by logging in using User and Password
 *
 * @param user MySQL user
 * @param password MySQL session password
 * @return int Bytes sent through TCP socket
 */

int MySQL::send_authentication_packet(const char *user, const char *password, const char *db) {

    int size_send = 4;
    memset(tcp_socket_buffer, 0, BUFF_SIZE);

    // client flags
    tcp_socket_buffer[size_send] = byte(0x0D);
    tcp_socket_buffer[size_send + 1] = byte(0xa6);
    tcp_socket_buffer[size_send + 2] = byte(0x03);
    tcp_socket_buffer[size_send + 3] = byte(0x00);
    size_send += 4;

    // max_allowed_packet
    tcp_socket_buffer[size_send] = 0;
    tcp_socket_buffer[size_send + 1] = 0;
    tcp_socket_buffer[size_send + 2] = 0;
    tcp_socket_buffer[size_send + 3] = 1;
    size_send += 4;

    // charset - default is 8
    tcp_socket_buffer[size_send] = byte(0x08);
    size_send += 1;
    for (int i = 0; i < 24; i++)
        tcp_socket_buffer[size_send + i] = 0x00;
    size_send += 23;

    // user name
    memcpy((char *)&tcp_socket_buffer[size_send], user, strlen(user));
    size_send += strlen(user) + 1;
    tcp_socket_buffer[size_send - 1] = 0x00;

    // password - see scramble password
    if (scramble_password(password, mSeed)) {
        tcp_socket_buffer[size_send] = 0x14;
        size_send += 1;
        for (int i = 0; i < 20; i++)
        tcp_socket_buffer[i + size_send] = mSeed[i];
        size_send += 20;
        tcp_socket_buffer[size_send] = 0x00;
    }

    if (db) {
        memcpy((char *)&tcp_socket_buffer[size_send], db, strlen(db));
        size_send += strlen(db) + 1;
        tcp_socket_buffer[size_send - 1] = 0x00;
    }
    else {
        tcp_socket_buffer[size_send + 1] = 0x00;
        size_send += 1;
    }

    // Write packet size
    int p_size = size_send - 4;
    store_int(&tcp_socket_buffer[0], p_size, 3);
    tcp_socket_buffer[3] = byte(0x01);

    // Write the packet
    size_t ret = write((char *)tcp_socket_buffer, size_send);
    flush_packet();
    return ret;
}


/**
 * @brief Cleans TCP socket
 *
 */
void MySQL::flush_packet()
{
    uint8_t *data_rec = nullptr;
    int packet_len = 0;

    data_rec = (uint8_t *)malloc(BUFF_SIZE);
    packet_len = client->readBytes(data_rec, BUFF_SIZE);
    packet_len -= 4;

    // Check for valid packet.
    if (packet_len < 0)
        packet_len = 0;

    for (int i = 4; i < packet_len + 4; i++)
        tcp_socket_buffer[i] = data_rec[i];

    delete data_rec;
}


/**
 * @brief Parse handshake sent by server
 *
*/
void MySQL::parse_handshake_packet()
{
    int i = 5;
    do {
        i++;
    } while (tcp_socket_buffer[i - 1] != 0x00);

    server_version = (char *)malloc(i - 5);
    strncpy(server_version, (char *)&tcp_socket_buffer[5], i - 5);

    // Capture the first 8 characters of seed
    i += 4; // Skip thread id
    for (int j = 0; j < 8; j++)
        mSeed[j] = tcp_socket_buffer[i + j];

    // Capture rest of seed
    i += 27; // skip ahead
    for (int j = 0; j < 12; j++)
        mSeed[j + 8] = tcp_socket_buffer[i + j];
}



#if DEBUG
void MySQL::print_packets_types(void)
{
    // Parse recieved tables
    for (int i = 0; i < (int)(this->mPacketsRecieved.size()); i++)
    {
        MySQL_Packet *packet = this->mPacketsRecieved.at(i);
        uint32_t packet_number = packet->mPacketNumber;
        Packet_Type packet_type = packet->getPacketType();
        String type;

        switch (packet_type)
        {
        case PACKET_OK:
            type = "OK";
            break;

        case PACKET_UNKNOWN:
            type = "UNKNOWN";
            break;

        case PACKET_TEXTRESULTSET:
            type = "TEXTRESULTSET";
            break;

        case PACKET_EOF:
            type = "EOF";
            break;

        case PACKET_ERR:
            type = "ERR";
            break;
        }
        // Serial.printf("Packet %d is a %s packet\r\n", packet_number, type.c_str());
    }
}
#endif