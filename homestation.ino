// An implementation of an ESP8266 sensor station for home automation.
// OSdZ

/*  Current Setup:
 *    Server: ESP8266
 *    Temp Sensor: DHT-22 (AM2302)
 *    LCD: Sparkfun 16x2 SerLCD
 */

#define DEBUG false  // Make false to disable Serial monitor debug messages.
#define DEBUG_SERIAL if(DEBUG)Serial

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>             // I2C to handle LCD
#include <SerLCD.h>           // Sparkfun LCD library
#include <DHT.h>              // Adafruit DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
#include <PageBuilder.h>      // Library to assemble HTML strings: https://github.com/Hieromon/PageBuilder

#include "htmlContents.h"     // Templates for HTML page bodies
#include "localSettings.h"    // Settings for local implementation, hidden from public repository
/* localSettings.h must include:
 * #define WIFI_SSID     "Network"
 * #define WIFI_PASSWORD "Pa$$w0rd"
 */

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

#define HTTP_REST_PORT    8080    // Port for REST server
#define DHTPIN            13      // Digital pin connected to the DHT sensor
#define SDAPIN            4       // I2C SDA pin
#define SCLPIN            5       // I2C SCL pin
#define DHTTYPE           DHT22   // DHT 22 (AM2302)
#define DHT_READ_INTERVAL 60000   // How often humidity and temperature are read and saved in milliseconds
#define DISPLAY_ADDRESS   0x72    // I2C address of the OpenLCD
#define DISPLAY_LENGTH    16      // Caracters in row

#define ROOM_NAME ("Living Room") // Room label for current implementation

SerLCD lcd;                              // Create LCD object
ESP8266WebServer server(HTTP_REST_PORT); // Create HTTP server object
DHT dht(DHTPIN, DHTTYPE);                // Create DHT sensor object

// Structure to hold room data.
struct Room {
  float hum;                        // Humidity
  float tempC;                      // Celsius temperature
  float tempF;                      // Fahrenheit temperature
  unsigned long conditionsRead = 0; // Timestamp for conditions read frequency
  char conditions[16];              // Humidity and temperature string for display
  char label[20];                   // Room name
} room;

bool hadClient = false; // Flag for flow control when client detected


/////////////////////////////////////////
//  HTTP Server Functions
/////////////////////////////////////////

// Functions to interpolate variables in HTML body
String setRoomLabel(PageArgument& args) {
	return String(room.label);
}

String setHum(PageArgument& args) {
	return String((int)(room.hum + .5)); // Value is rounded for display
}

String setTempC(PageArgument& args) {
	return String((int)(room.tempC + .5)); // Value is rounded for display
}

String setTempF(PageArgument& args) {
	return String((int)(room.tempF + .5)); // Value is rounded for display
}

String setAge(PageArgument& args) {
  unsigned long age = millis() - room.conditionsRead;
  unsigned int secs = (int)(age / 1000) % 60;
  unsigned int mins = (int)(age / 1000) / 60;
  char formattedAge[8]; // millis overflows at age 71582:47 mins:secs
  sprintf(formattedAge,"%dm %02ds",mins,secs);
	return String(formattedAge);
}

// PageBuilder Elements
PageElement body_root( HTML_ROOT, {
  {"TKN_RM_LABEL", setRoomLabel},
  {"TKN_HUMIDITY", setHum},
  {"TKN_TEMP_C", setTempC},
  {"TKN_TEMP_F", setTempF},
  {"TKN_AGE", setAge}
});
PageBuilder page( "/", {body_root} );

PageElement body_notFound(HTML_NOTFOUND);
PageBuilder notFound( {body_notFound} );

// TODO: Can this be implemented without using String objects?

/////////////////////////////////////////
//   Display Functions
/////////////////////////////////////////

// Clear display (and optionally turn off backlight).
void displayClear(bool lightOff = true) {

  lcd.clear();                      // Clear screen, cursor to home
  if (lightOff == true) {
    lcd.setFastBacklight(0x000000); // Backlight Off
  }
}

// Clear selected row and display message (0: top row, 1: bottom row)
void displayRow(char row, char *message) {

  lcd.setCursor(0, row);  // Move cursor to beginning of given row

  // TODO: Handle case where message > DISPLAY_LENGTH

  lcd.write(message);
  if (strlen(message) < DISPLAY_LENGTH) {
    for (int i = strlen(message); i < DISPLAY_LENGTH; i++) {
      lcd.write(" ");
    }
  }
}


/////////////////////////////////////////
//   DHT Functions
/////////////////////////////////////////

// Read DHT values and update room struct
bool getConditions() {

  // Only read DHT if DHT_READ_INTERVAL has passed
  unsigned long now =  millis();
  if (now - room.conditionsRead > DHT_READ_INTERVAL || room.conditionsRead == 0) {
    room.hum = dht.readHumidity(); // Humidity
    room.tempC = dht.readTemperature(); // Celsius
    room.tempF = dht.readTemperature(true); // Fahrenheit
  } else {
    return true; // No update necessary
  }

  // Check if any reads failed.
  if (isnan(room.hum) || isnan(room.tempC) || isnan(room.tempF)) {

    DEBUG_SERIAL.println(F("Failed to read from DHT sensor!"));
    return false; // Read failed.

  } else {

    room.conditionsRead = now;

    // Preview sensor read values.
    DEBUG_SERIAL.printf("Read H:%f, T:%f and F:%f from DHT sensor on %d.\n", room.hum, room.tempC, room.tempF, now);

    // TODO: Move following display code out of this function.
    displayRow(1,"                ");                                     // Clear bottom row
    lcd.setCursor(0, 1);                                                  // Move cursor to beginning of row 1
    lcd.printf("%d%% %d", (int)(room.hum + .5), (int)(room.tempC + .5));  // Display sensor values.
    lcd.write(0xDF);                                                      // Degree symbol
    lcd.printf("C(%d", (int)(room.tempF + .5));                           // Display sensor values.
    lcd.write(0xDF);                                                      // Degree symbol
    lcd.write("F)");

    return true; // Read succeeded.

  }
}



/////////////////////////////////////////
//   Setup
/////////////////////////////////////////

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Built-in LED used as client connection indicator
  strcpy(room.label, ROOM_NAME);    // Current implementation room

  DEBUG_SERIAL.begin(9600);
  DEBUG_SERIAL.println();

  // Initialize LCD display on I2C
  Wire.begin(SDAPIN, SCLPIN);
  lcd.begin(Wire, DISPLAY_ADDRESS);
  delay(1000);                    // Wait for LCD to stabilize
  lcd.clear();                    // Clear screen, cursor to home
  lcd.setFastBacklight(0x000000); // Backlight Off

  dht.begin();

  WiFi.mode(WIFI_STA);

  DEBUG_SERIAL.printf("Connecting to %s ", ssid);
  displayRow(0, "Connecting...");
  lcd.setFastBacklight(0x000040);; // Backlight to blue
  WiFi.begin(ssid, password);

  char blinker = 0;
  while (WiFi.status() != WL_CONNECTED) {

    delay(750);
    DEBUG_SERIAL.print(".");

    lcd.setCursor(14, 0);                                  // Move cursor to Col 14 Row 0
    (blinker % 2 == 0) ? lcd.write(0xBC) : lcd.write(" ");  // Happy wait
    blinker++;
  }

  DEBUG_SERIAL.println(F(" Connected!"));
  displayRow(0, "Connected!");
  lcd.setFastBacklight(0x000000); // Backlight Off

  // Set server routing
  page.insert(server);
  // TODO: implement exitCanHandle() method to reduce memory usage by PageBuilder elements.
  notFound.atNotFound(server);


  // Start server
  server.begin();
  DEBUG_SERIAL.printf("Web server started, open %s:%u in a web browser\n", WiFi.localIP().toString().c_str(),HTTP_REST_PORT);

  lcd.setCursor(0, 0); // Move cursor to beginning of Row 0
  lcd.write(0x40); // @
  lcd.printf("%s", WiFi.localIP().toString().c_str()); //Display IP

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}



/////////////////////////////////////////
//   Loop
/////////////////////////////////////////

void loop(void) {

  while (!getConditions()); // TODO: Handle thhe case where DHT fails to read to keep this loop from hanging.

  server.handleClient();

  if (server.client() && !hadClient) {
    DEBUG_SERIAL.println("Client connected!");
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on

    displayRow(0, "Client Connected");
    lcd.setFastBacklight(0x004000); // Backlight to Green

    hadClient = true;

  } else if (!server.client() && hadClient) {
    DEBUG_SERIAL.println("Client disconnected.");
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off

    displayRow(0, "Client left");
    lcd.setFastBacklight(0x000000); // Backlight Off

    hadClient = false;
  }
  // TODO: Clear "Client Left" message after timeout and return display to IP address
}
