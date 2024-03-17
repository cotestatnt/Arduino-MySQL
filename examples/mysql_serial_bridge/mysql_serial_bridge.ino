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
char sql_str[MAX_QUERY_LEN];

// Variadic function that will execute the query selected with passed parameters
bool queryExecute(DataQuery_t& data, const char* queryStr, ...) {
  if (sql.connected()) {
    char buf[MAX_QUERY_LEN];
    va_list args;
    va_start (args, queryStr);
    vsnprintf (buf, sizeof(buf), queryStr, args);
    va_end (args);

    Serial.printf("\nExecuting SQL query: %s\n", buf);
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

  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  //Open MySQL session
  Serial.print("Connecting to... ");
  Serial.println(dbHost);

	if (sql.connect(user, password, database)) {
    Serial.println();
  }
  delay(2000);
  Serial.println("Enter a valid SQL statement:");
}

void loop() {
  bool doQuery = false, printHeap = false;
  if (Serial.available()) {
    int pos = 0;

    // Read all characters from serial until new line or carriage return
    while (Serial.available()) {
      char ch = (char) Serial.read();
      // Skip new line and carriage return
      if (ch != '\n' && ch != '\r') {
        // Add escape to single quote
        if (ch == '\'') {
          sql_str[pos++] = '\\';
        }
        sql_str[pos++] = ch;
      }
      else {
        sql_str[pos] = '\0';  // Add string terminator
        doQuery = true;       // Ready to run SQL query
      }
    }
  }

  // Execute the SQL query passed with serial monitor
  if (doQuery) {
    doQuery = false;

    // Create a DataQuery_t object for store query results
    DataQuery_t data;
    if (queryExecute(data, sql_str)){
      Serial.println("Query executed.");

      // Check if the query has some records
      if (data.recordCount) {
        // Print formatted content of table
        sql.printResult(data, Serial);
        Serial.print('\n');
      }
    }

    // Print heap outside so we are sure 'DataQuery_t data' was destroyed and RAM has been freed
    printHeap = true;
  }

  if (printHeap) {
    printHeap = false;
    // Check heap status (only ESP)
    printHeapStats();
  }

}



// Print some heap status information (only for ESP MCUs)
void printHeapStats() {
#ifdef ESP32
    Serial.printf("Total free: %6d - Max block: %6d\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
#elif defined(ESP8266)
    uint32_t free;  uint16_t max;
    ESP.getHeapStats(&free, &max, nullptr);
    Serial.printf("Total free: %5d - Max block: %5d\n", free, max);

#else
    // AVR and others platform
    // https://github.com/mpflaga/Arduino-MemoryFree
    Serial.print("Free RAM: ");
    #ifdef __arm__
    // should use uinstd.h to define sbrk but Due causes a conflict
    extern "C" char* sbrk(int incr);
    #else  // __ARM__
    extern char *__brkval;
    #endif  // __arm__
      char top;
    #ifdef __arm__
      Serial.println( &top - reinterpret_cast<char*>(sbrk(0)));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
      Serial.println( &top - __brkval);
    #else  // __arm__
      Serial.println( __brkval ? &top - __brkval : &top - __malloc_heap_start);
    #endif  // __arm__
#endif
}