// An implementation of an ESP8266 sensor station for home automation.
// OSdZ

/*  Current Setup:
 *    Server: ESP8266
 *    Temp Sensor: DHT-22 (AM2302)
 */

#include <ESP8266WebServer.h>
#include "DHT.h"            // Adafruit DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library

#include "localSettings.h"  // Settings for local WiFi access, hidden from public repository
/*
 * #define WIFI_SSID     "Network"
 * #define WIFI_PASSWORD "Pa$$w0rd"
 */
#define HTTP_REST_PORT 8080 // Port for REST server
#define DHTPIN 4            // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22       // DHT 22 (AM2302)

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

ESP8266WebServer server(HTTP_REST_PORT); // Initialize server
DHT dht(DHTPIN, DHTTYPE);          // Initialize sensor
float h, t;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Built-in LED used as client connection indicator
  
  Serial.begin(115200);
  Serial.println();

  dht.begin();

  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F(" Connected!"));

  // Set server routing
  serverRouting();
  // Start server
  server.begin();
  Serial.printf("Web server started, open %s:%u in a web browser\n", WiFi.localIP().toString().c_str(),HTTP_REST_PORT);

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off
}

void handleRoot()
{
  String htmlBody;
  htmlBody.reserve(1024);                // prevent RAM fragmentation
  htmlBody = F("Humidity: ");
  htmlBody += (int)(h + .5);               // round displayed humidity
  htmlBody += F(" &percnt;<br>"
                "Temperature: ");
  htmlBody += (int)(t + .5);               // round displayed temperature
  htmlBody += F(" &deg;C");
  server.sendHeader("Connection", "close");
  server.send(200, F("text/html"), htmlBody);
}

// Manage not found URL
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

// Define routing
void serverRouting() {
  server.on("/", HTTP_GET, handleRoot);

  server.on("/test", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, F("text/html"), "&quot;Hello!&quot;");
  });
  
  // Set not found response
  server.onNotFound(handleNotFound);
}

void loop(void) {
  
  delay(2000);
  h = dht.readHumidity();
  t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.printf("Read H:%f and T:%f from DHT sensor.\n", h, t); // Preview sensor read values.
  
  server.handleClient();
}
