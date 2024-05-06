const char* ssid = "xxxxxx";		          // WiFi SSID
const char* wifiPwd = "xxxxxx";               // WiFi password
 
const char* user = "defaultdb";                // MySQL user login username
const char* password = "xxxxxxxx";             // MySQL user login password
const char* dbHost = "tby.h.filess.io";        // MySQL hostname or IP address

const char* database = "defaultdb";            // Database name
const char* table = "gpios";                   // Table name
uint16_t dbPort = 3305;                        // MySQL host port
uint32_t pollTime = 5000;                      // Waiting time between one request and the next


static const char CREATE_TABLE_SQL[] PROGMEM = R"string_literal(
CREATE TABLE `%s` (
  `gpio` int(11) NOT NULL,
  `type` int(11) DEFAULT 0,
  `state` tinyint(1) DEFAULT 0,
  `label` varchar(32) DEFAULT '',
  PRIMARY KEY (`gpio`)
);
)string_literal";