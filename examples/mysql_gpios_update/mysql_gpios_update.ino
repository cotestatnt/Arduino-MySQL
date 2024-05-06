#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
 #include <WiFi.h>
#endif

#include <MySQL.h>
#include "secrets.h"

#define PRINT_SQL     1
#define LED           RGB_BUILTIN


struct Gpio_t {
  uint8_t pin;
  uint8_t type;
  uint8_t state;
  const char* label;
};

Gpio_t gpios[] = {
  {0, INPUT_PULLUP, HIGH, "Boot Button"},
  {4, INPUT_PULLUP, HIGH, "User Button1"},
  {5, INPUT_PULLUP, HIGH, "User Button2"},
  {6, OUTPUT, LOW, "Test Output"},
  {LED_BUILTIN, OUTPUT, LOW, "Rgb LED"}
};

WiFiClient client;
MySQL sql(&client, dbHost, dbPort);
#define MAX_QUERY_LEN 256

// Variadic function that will execute the query selected with passed parameters
bool queryExecute(DataQuery_t& data, const char* queryStr, ...) {
  if (sql.connected()) {
    char buf[MAX_QUERY_LEN];
    va_list args;
    va_start (args, queryStr);
    vsnprintf (buf, sizeof(buf), queryStr, args);
    va_end (args);

#if PRINT_SQL
    Serial.printf("Executing SQL query: %s\n", buf);
#endif
    // Execute the query
    return sql.query(data, buf);
  }
  return false;
}

// Read the state of outputs from DB and set level 
void updateOutputs() {

  // NON-blocking delay of pollTime
  static uint32_t lastPollTime = millis();
  if (millis() - lastPollTime > pollTime) {
    lastPollTime = millis();

    // Start new connection to DB
    sql.connect(user, password, database, true);

    // Create a DataQuery_t object for store query results
    DataQuery_t data;
    if (queryExecute(data, "SELECT * FROM %s WHERE type = %d", table, OUTPUT)){

      // Print formatted content of table
      // sql.printResult(data, Serial);
      // Serial.print('\n');
      
      // Update output state
      for (int row = 0; row < data.recordCount; row++) { 
        String type = data.getRowValue(row, "type");
        String pin = data.getRowValue(row, "gpio");
        String level = data.getRowValue(row, "state");
        if (type.toInt() == OUTPUT) {
          digitalWrite(pin.toInt(), level.toInt());
        }
      }
      
      // Close the connection
      sql.disconnect();
      Serial.println();
    }
  }
}

// Update the state of inputs into the DB table
void updateInputs() {
  static uint16_t lastGpioState = 0;
  uint16_t gpioState;

  int elem = 0;     // Number of elements in gpios[] array
  for (Gpio_t &gpio : gpios) {
    gpio.state = digitalRead(gpio.pin);
    gpioState |= (gpio.state << elem++);    
  }

  // Update the remote table only when one of the inputs has changed
  if (gpioState != lastGpioState) {
    lastGpioState = gpioState;

    String strSql;  
    for (Gpio_t &gpio : gpios) {
      if (gpio.type == INPUT_PULLUP || gpio.type == INPUT) {
        char rowSql[64];   
        snprintf(rowSql, sizeof(rowSql), "UPDATE %s SET state = %d WHERE gpio = %d;\n", table, gpio.state, gpio.pin);
        strSql += rowSql;
      }
    }

    // Start new connection to DB
    sql.connect(user, password, database, true);

    // Do the SQL update query
    DataQuery_t data;
    queryExecute(data, strSql.c_str());

    // Close the connection
    sql.disconnect();
  }
}

// If not exist, create table and then insert records with defined gpios
void createUpdateTable() {
  // Start new connection to DB
  if (sql.connect(user, password, database)) {
    delay(500);

    // Create table if not exists
    Serial.println("Create table if not exists");
    DataQuery_t data;
    if (!queryExecute(data, CREATE_TABLE_SQL, table)) {
      Serial.println("CREATE query error. Table already defined");
    }    

    // Prepare the SQL string with each gpio record
    String strSql = "INSERT IGNORE INTO " + String(table) + " (gpio, type, state, label) VALUES ";
    for (Gpio_t &gpio : gpios) {
      char rowSql[64];   
      snprintf(rowSql, sizeof(rowSql), "(%d, %d, %d, '%s'),", gpio.pin, gpio.type, gpio.state, gpio.label);
      strSql += rowSql;
    }
    strSql[strSql.length()-1] = ';';      // Replace last ',' with ';'
    
    // Do the SQL insert/update query
    delay(500);
    data.clear();
    queryExecute(data, strSql.c_str());

    // Close the connection
    sql.disconnect();
  }
}


void setup() {
  for (Gpio_t &gpio : gpios) {
    pinMode(gpio.pin, gpio.type);
  }
  Serial.begin(115200);
  Serial.println("******************************************************");
  Serial.print("Connecting to WiFI ");

  WiFi.begin(ssid, wifiPwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Open MySQL session
  Serial.print("Connecting to... ");
  Serial.println(dbHost);

  createUpdateTable();
}

void loop() {
  // Read the state of outputs from DB and set level of gpio
  updateOutputs();
  
  // Update the state of gpio inputs into the DB table
  updateInputs();
}

