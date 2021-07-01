// An implementation of an ESP8266 sensor station for home automation.
// OSdZ

/*  Current Setup:
 *    Server: ESP8266
 *    Temp Sensor: DHT-22 (AM2302)
 *    LCD: Sparkfun 16x2 SerLCD
 */

#include <ESP8266WebServer.h>
#include <Wire.h>            // I2C to handle LCD
#include <DHT.h>             // Adafruit DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library

#include "localSettings.h"   // Settings for local WiFi access, hidden from public repository
/*
 * #define WIFI_SSID     "Network"
 * #define WIFI_PASSWORD "Pa$$w0rd"
 */
#define HTTP_REST_PORT 8080   // Port for REST server
#define DHTPIN 13             // Digital pin connected to the DHT sensor
#define SDAPIN 4              // I2C SDA pin
#define SCLPIN 5              // I2C SCL pin
#define DHTTYPE DHT22         // DHT 22 (AM2302)
#define DISPLAY_ADDRESS 0x72  // This is the default address of the OpenLCD
#define DISPLAY_LENGTH 16     // Caracters in row

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

ESP8266WebServer server(HTTP_REST_PORT); // Initialize server
DHT dht(DHTPIN, DHTTYPE);                // Initialize sensor

float h, t, f; // Variables to store Temp and Humidity values.
bool hadClient = false; // Flag for flow control when client detected.

/////////////////////////////////////////
// Server Functions 
/////////////////////////////////////////

void handleRoot()
{
  String htmlBody;
  htmlBody.reserve(1024);                // prevent RAM fragmentation
  htmlBody = F("Humidity: ");
  htmlBody += (int)(h + .5);               // round displayed humidity
  htmlBody += F(" &percnt;<br>"
                "Temperature: ");
  htmlBody += (int)(t + .5);               // round displayed temperature
  htmlBody += F(" &deg;C (");
  htmlBody += (int)(f + .5);               // round displayed temperature
  htmlBody += F(" &deg;F)");
  server.sendHeader("Connection", "close");
  server.send(200, F("text/html"), htmlBody);
}

void handleNotFound() {
  String message;
  message.reserve(1024);
  message += "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void serverRouting() {
  server.on("/", HTTP_GET, handleRoot);

  server.on("/test", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, F("text/html"), "&quot;Hello!&quot;");
  });
  
  // Set not found response
  server.onNotFound(handleNotFound);
}

/////////////////////////////////////////
// Display Functions 
/////////////////////////////////////////

// Set Display backlight color
void displayRGB(char r, char g, char b) {
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write('|'); // Put LCD into setting mode
  Wire.write('+'); // Set RGB mode
  Wire.write(r); // R byte
  Wire.write(g); // G byte
  Wire.write(b); // B byte
  Wire.endTransmission(); 
}

// Clear display (and optionally turn off backlight).
void displayClear(bool lightOff = true) {
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write('|'); // Put LCD into setting mode
  Wire.write('-'); // Send clear display command
  if (lightOff == true) {
    Wire.write('|'); // Put LCD into setting mode
    Wire.write('+'); // Set RGB mode
    Wire.write(0); // R byte
    Wire.write(0); // G byte
    Wire.write(0); // B byte
  }
  Wire.endTransmission();
}

// Clear selected row and display message (0: top row, 1: bottom row)
void displayRow(char row, char *message) {
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(254); // Special Command
  
  if (row == 0) {
    Wire.write(128); // Move cursor to L0 S0
  } else if (row == 1) {
    Wire.write(128 + 64); // Move cursor to L1 S0
  }
  Wire.print(message);
  if (strlen(message) < DISPLAY_LENGTH) {
    for (int i = strlen(message); i < DISPLAY_LENGTH; i++) {
      Wire.print(" ");
    }
  }
  Wire.endTransmission();
}

/////////////////////////////////////////
// Setup
/////////////////////////////////////////

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Built-in LED used as client connection indicator
  
  Serial.begin(115200);
  Serial.println();

  Wire.begin(SDAPIN, SCLPIN); // Initialize I2C for LCD display
  displayClear();
  
  dht.begin();

  WiFi.mode(WIFI_STA);
  
  Serial.printf("Connecting to %s ", ssid);
  displayRow(0, "Connecting...");
  displayRGB(0, 0, 0x40); // Backlight to blue
  WiFi.begin(ssid, password);
  
  char blinker = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    Wire.beginTransmission(DISPLAY_ADDRESS);
    Wire.write(254); // Special Command
    Wire.write(128 + 14); // Move cursor to L0 S14
    Wire.endTransmission();
    (blinker % 2 == 0) ? Wire.write(0xBC) : Wire.write(" ");
    blinker++;
    
  }
  
  Serial.println(F(" Connected!"));
  displayRow(0, "Connected!");
  displayRGB(0, 0, 0); // Backlight off

  // Set server routing
  serverRouting();
  
  // Start server
  server.begin();
  Serial.printf("Web server started, open %s:%u in a web browser\n", WiFi.localIP().toString().c_str(),HTTP_REST_PORT);

  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(254); // Special Command
  Wire.write(128); // Move cursor to L0 S0
  Wire.write(0x40); // @
  Wire.printf("%s", WiFi.localIP().toString().c_str()); //Display IP
  Wire.endTransmission();

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}

/////////////////////////////////////////
// Loop
/////////////////////////////////////////

void loop(void) {

  delay(2000);
  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.printf("Read H:%f, T:%f and F:%f from DHT sensor.\n", h, t, f); // Preview sensor read values.

  displayRow(1," "); // Clear bottom row
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(254); // Special Command
  Wire.write(128 + 64); // Move cursor to L1 S0
  Wire.printf("%d%% %d", (int)(h + .5), (int)(t + .5)); // Display sensor values.
  Wire.write(0xDF); // Degree symbol
  Wire.printf("C(%d", (int)(f + .5)); // Display sensor values.
  Wire.write(0xDF); // Degree symbol
  Wire.print("F)");
  Wire.endTransmission();
  
  server.handleClient();

  if (server.client()) {
    Serial.println("Client connected!");
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on

    displayRow(0, "Client Connected");
    displayRGB(0, 0x40, 0); // Backlight to Green

    hadClient = true;
    
  } else if (hadClient) {
    Serial.println("Client disconnected.");
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
    
    displayRow(0, "Client left");
    displayRGB(0, 0, 0); // Backlight off

    hadClient = false;
  }
}
