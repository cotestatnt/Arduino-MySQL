#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
 #include <WiFi.h>
#endif

#include <MySQL.h>
#include "secrets.h"

WiFiClient client;
MySQL sql(&client, dbHost, dbPort);
#define MAX_QUERY_LEN 128

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
  delay(2000);
}

void loop() {
  // Create a DataQuery_t object for store query results
  DataQuery_t data;
  if (queryExecute(data, "SELECT * FROM %s", table)){
    Serial.println("Query executed.");
    if (data.recordCount) {
      // Print formatted content of table
      sql.printResult(data);
      Serial.print('\n');

      /*
      * data.fields is a std::vector<Field_t> object (Field_t defined in DataQuery.h)
      * which you can manually iterate using a range based for loop for easy data parsing
      */
      for (Field_t field : data.fields) {
        Serial.printf("%s (%d), ", field.name.c_str(), field.size);
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
          // String value = data.records.record.at(i).c_str();
          Serial.printf("%s, ", value.c_str());
        }
        Serial.print('\n');
      }

      // for (Record_t rec : data.records, i++) {
      //   for (int col = 0; col < data.fieldCount; col++) {
      //     Serial.printf("%s, ", rec.record.at(i).c_str());
      //   }
      //   Serial.print('\n');
      // }
    }
  }
  Serial.print('\n');
  printHeapStats();
  delay(pollTime);
}


// Print some heap status information (only for ESP MCUs)
void printHeapStats() {
  static uint32_t heapTime;
  if (millis() - heapTime > 5000) {
    heapTime = millis();
#ifdef ESP32
    Serial.printf("Total free: %6d - Max block: %6d\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
#elif defined(ESP8266)
    uint32_t free;  uint16_t max;
    ESP.getHeapStats(&free, &max, nullptr);
    Serial.printf("Total free: %5d - Max block: %5d\n", free, max);
#else
    ;   // Do nothing
#endif
  }
}
