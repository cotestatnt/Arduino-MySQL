#include <WiFi.h>
#include <MySQL.h>
#include "secrets.h"

WiFiClient client;
MySQL sql(&client, dbHost, dbPort);
#define MAX_QUERY_LEN 512

const char* table = "reset_reasons";               // Table name


#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */


const char* getResetReason(int reason) {
  switch ( reason) {
    case 1  : return ("Vbat power on reset");break;
    case 3  : return ("Software reset digital core");break;
    case 4  : return ("Legacy watch dog reset digital core");break;
    case 5  : return ("Deep Sleep reset digital core");break;
    case 6  : return ("Reset by SLC module, reset digital core");break;
    case 7  : return ("Timer Group0 Watch dog reset digital core");break;
    case 8  : return ("Timer Group1 Watch dog reset digital core");break;
    case 9  : return ("RTC Watch dog Reset digital core");break;
    case 10 : return ("Instrusion tested to reset CPU");break;
    case 11 : return ("Time Group reset CPU");break;
    case 12 : return ("Software reset CPU");break;
    case 13 : return ("RTC Watch dog Reset CPU");break;
    case 14 : return ("for APP CPU, reseted by PRO CPU");break;
    case 15 : return ("Reset when the vdd voltage is not stable");break;
    case 16 : return ("RTC Watch dog reset digital core and rtc module");break;
    default : return ("NO_MEAN");
  }
}


static const char createQuery[] PROGMEM = R"string_literal(
CREATE TABLE `%s` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `MAC address` VARCHAR(18) NOT NULL,
  `CPU0 reset reason` TEXT NULL,
  `CPU1 reset reason` TEXT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `timestamp` (`timestamp`)
) ENGINE=INNODB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;
)string_literal";

static const char insertQuery[] PROGMEM = R"string_literal(
INSERT INTO `%s` (`MAC address`, `CPU0 reset reason`, `CPU1 reset reason`) VALUES ('%s', '%s', '%s');
)string_literal";


// Variadic function that will execute the query selected with passed parameters
bool queryExecute(DataQuery_t& data, const char* queryStr, ...) {
  if (sql.connected()) {
    char buf[MAX_QUERY_LEN];
    va_list args;
    va_start (args, queryStr);
    vsnprintf (buf, sizeof(buf), queryStr, args);
    va_end (args);

    Serial.printf("Executing SQL query: %s\n", buf);
    // Execute the query
    return sql.query(data, buf);
  }
  return false;
}


void setup() {
  Serial.begin(115200);
  Serial.println("******************************************************");
  Serial.print("Connecting to WiFI");

  WiFi.begin(ssid, wifiPwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //Open MySQL session
  Serial.print("Connecting to... ");
  Serial.println(dbHost);

	if (sql.connect(user, password, database)) {
    Serial.println();
  }

  // Create table if not exists
  Serial.println("\nCreate table if not exists");
  DataQuery_t data;
  if (!queryExecute(data, createQuery, table)) {
    Serial.println("CREATE query error. Table already defined");
  }
  data.clear();

  // Insert a record
  Serial.print("CPU0 reset reason: ");
  Serial.println(getResetReason(rtc_get_reset_reason(0)));

  Serial.print("CPU1 reset reason: ");
  Serial.println(getResetReason(rtc_get_reset_reason(1)));
  Serial.println();

  if (queryExecute( data, insertQuery, table, 
      WiFi.macAddress().c_str(),
      getResetReason(rtc_get_reset_reason(0)),
      getResetReason(rtc_get_reset_reason(1)))
     ) 
  {
    Serial.println("INSERT query executed. New record added to table");
  }
  Serial.println();
  delay(2000);
}

void loop() {
  // Create a DataQuery_t object for store query results
  DataQuery_t data;

  // Select last 10 records and print them
  if (queryExecute(data, "SELECT * FROM %s ORDER BY id DESC LIMIT 10", table)){
    Serial.println("SELECT query executed.");
    if (data.recordCount) {
      // Print formatted content of table
      sql.printResult(data);
      Serial.print('\n');
    }
  }
  Serial.print('\n');
  delay(pollTime);
}
