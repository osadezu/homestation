// An implementation of an ESP8266 sensor station for home automation.
// OSdZ

/*  Current Setup:
 *    Server: ESP8266
 *    Temp Sensor: DHT-22 (AM2302)
 *    LCD: Sparkfun 16x2 SerLCD
 */

#include <ESP8266WebServer.h>
#include <Wire.h>            // I2C to handle LCD
#include <SerLCD.h>          // Sparkfun LCD library
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

SerLCD lcd;                              // Initialize LCD object
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
  
  ////// TO-DO: Handle case where message > DISPLAY_LENGTH //////
  
  lcd.write(message);
  if (strlen(message) < DISPLAY_LENGTH) {
    for (int i = strlen(message); i < DISPLAY_LENGTH; i++) {
      lcd.write(" ");
    }
  }
}

/////////////////////////////////////////
// Setup
/////////////////////////////////////////

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Built-in LED used as client connection indicator
  
  Serial.begin(115200);
  Serial.println();

  // Initialize LCD display on I2C
  Wire.begin(SDAPIN, SCLPIN); 
  lcd.begin(Wire, DISPLAY_ADDRESS); 
  lcd.clear();                    // Clear screen, cursor to home
  lcd.setFastBacklight(0x000000); // Backlight Off
  
  dht.begin();

  WiFi.mode(WIFI_STA);
  
  Serial.printf("Connecting to %s ", ssid);
  displayRow(0, "Connecting...");
  lcd.setFastBacklight(0x000040);; // Backlight to blue
  WiFi.begin(ssid, password);
  
  char blinker = 0;
  while (WiFi.status() != WL_CONNECTED) {
    
    delay(750);
    Serial.print(".");
    
    lcd.setCursor(14, 0); // Move cursor to Col 14 Row 0
    (blinker % 2 == 0) ? lcd.write(0xBC) : lcd.write(" "); // Happy wait
    blinker++;
  }
  
  Serial.println(F(" Connected!"));
  displayRow(0, "Connected!");
  lcd.setFastBacklight(0x000000); // Backlight Off

  // Set server routing
  serverRouting();
  
  // Start server
  server.begin();
  Serial.printf("Web server started, open %s:%u in a web browser\n", WiFi.localIP().toString().c_str(),HTTP_REST_PORT);

  lcd.setCursor(0, 0); // Move cursor to beginning of Row 0
  lcd.write(0x40); // @
  lcd.printf("%s", WiFi.localIP().toString().c_str()); //Display IP
  
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

  displayRow(1,"                "); // Clear bottom row
  lcd.setCursor(0, 1); // Move cursor to beginning of row 1
  lcd.printf("%d%% %d", (int)(h + .5), (int)(t + .5)); // Display sensor values.
  lcd.write(0xDF); // Degree symbol
  lcd.printf("C(%d", (int)(f + .5)); // Display sensor values.
  lcd.write(0xDF); // Degree symbol
  lcd.write("F)");
  
  server.handleClient();

  if (server.client()) {
    Serial.println("Client connected!");
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on

    displayRow(0, "Client Connected");
    lcd.setFastBacklight(0x004000); // Backlight to Green

    hadClient = true;
    
  } else if (hadClient) {
    Serial.println("Client disconnected.");
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
    
    displayRow(0, "Client left");
    lcd.setFastBacklight(0x000000); // Backlight Off

    hadClient = false;
  }
}
