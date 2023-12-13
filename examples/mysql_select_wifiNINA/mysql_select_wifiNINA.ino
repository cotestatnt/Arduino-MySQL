#include <SPI.h>
#include <WiFiNINA.h>
#include <MySQL.h>
#include "secrets.h"

WiFiClient client;
MySQL sql(&client, dbHost, dbPort);
#define MAX_QUERY_LEN 128

int status = WL_IDLE_STATUS;

void setup() {
  Serial.begin(115200);
  Serial.println(F("******************************************************"));
  Serial.print(F("Connecting to WiFI"));

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("Communication with WiFi module failed!"));
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println(F("Please upgrade the firmware"));
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, wifiPwd);

    // wait 10 seconds for connection:
    delay(5000);
  }
  Serial.println(F("Connected to WiFi"));
  printWifiStatus();

  //Open MySQL session
  Serial.print(F("Connecting to... "));
  Serial.println(dbHost);

	if (sql.connect(user, password, database)) {
    Serial.println();
  }
  delay(2000);
}

void loop() {
  // Create a DataQuery_t object for store query results
  DataQuery_t data;
  char query[MAX_QUERY_LEN];
  snprintf(query, MAX_QUERY_LEN, "SELECT * FROM %s", table);
  if (sql.query(data, query )){
    Serial.println(F("Query executed."));
    if (data.recordCount) {
      // Print formatted content of table
      sql.printResult(data);
      Serial.print('\n');

      /*
      * data.fields is a std::vector<Field_t> object (Field_t defined in DataQuery.h)
      * which you can manually iterate using a range based for loop for easy data parsing
      */
      for (Field_t field : data.fields) {
        Serial.print(field.name.c_str());
        Serial.print(" (");
        Serial.print(field.size);
        Serial.print("), ");
      }
      Serial.print('\n');

      /*
      * data.records is a std::vector<Record_t> object (Record_t defined in DataQuery.h)
      * which you can manually iterate using a range based for loop as for fields
      * or as alternative you can iterate each record with a classic for loop
      */
      for (int row = 0; row < data.recordCount; row++) {
        for (int col = 0; col < data.fieldCount; col++) {
          String value = data.getRowValue(row, col);
          Serial.print(value);
          Serial.print(", ");
        }
        Serial.print('\n');
      }
    }
  }
  Serial.print('\n');

  Serial.print(F("Free RAM = ")); //F function does the same and is now a built in library, in IDE > 1.0.0
  Serial.println(freeMemory());  // print how much RAM is available in bytes.

  delay(pollTime);
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}




#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
