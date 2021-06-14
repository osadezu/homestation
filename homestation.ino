// An implementation of an ESP8266 sensor station for home automation.
// OSdZ

/*  Current Setup:
 *    Server: ESP8266
 *    Temp Sensor: DHT-22 (AM2302)
 */

#include <ESP8266WiFi.h>
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

WiFiServer server(HTTP_REST_PORT); // Initialize server
DHT dht(DHTPIN, DHTTYPE);          // Initialize sensor

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Built-in LED used as client connection indicator
  
  Serial.begin(115200);
  Serial.println();

  dht.begin();
  
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F(" Connected!"));

  server.begin();
  Serial.printf("Web server started, open %s:%u in a web browser\n", WiFi.localIP().toString().c_str(),server.port());

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
}

String prepareHtmlPage(float h, float t)
{
  String htmlPage;
  htmlPage.reserve(1024);                // prevent RAM fragmentation
  htmlPage = F("HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Connection: close\r\n"   // the connection will be closed after completion of the response
//               "Refresh: 10\r\n"         // refresh the page automatically every 10 seconds
               "\r\n"
               "<!DOCTYPE HTML>"
               "<html>"
               "Humidity: ");
  htmlPage += (int)(h+.5);               // round displayed humidity
  htmlPage += F(" &percnt;<br>"
                "Temperature: ");
  htmlPage += (int)(t+.5);               // round displayed temperature
  htmlPage += F(" &deg;C"
                "</html>");
  return htmlPage;
}

void loop() {
  WiFiClient client = server.available();

  delay(2000);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.printf("Read H:%f and T:%f from DHT sensor.\n", h, t); // Preview sensor read values.

  // wait for a client (web browser) to connect
  Serial.println(F("\n[Waiting for Client]"));
  if (client)
  {
    Serial.println(F("\n[Client connected]"));
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    while (client.connected())
    {
      // read line by line what the client (web browser) is requesting
      if (client.available())
      {
        String line = client.readStringUntil('\r');
        Serial.print(line);
        // wait for end of client's request, that is marked with an empty line
        if (line.length() == 1 && line[0] == '\n')
        {
          Serial.println(F("\n[Preparing HTML]"));
          client.println(prepareHtmlPage(h,t));
          break;
        }
      }
    }

    while (client.available()) {
      // let client finish its request
      delay(100);
      client.read();
    }

    // close the connection:
    client.stop();
    Serial.println(F("[Client disconnected]"));
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}
