#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "wifi-secrets.h"

String destTemp = "25.0"; // deg C
String treshold = "1";	  // 1 deg C
String lastTemperature;
String enableArmChecked = "checked";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Temperature Threshold Output Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body style="background: #f8f8ff; margin-top: 50px; text-align: center; font-family: sans-serif">

<svg xmlns="http://www.w3.org/2000/svg" height="140" width="300">
    <g id="lcd1" transform="scale(2 2)" fill="tomato"></g>
</svg>
<br>
<svg xmlns="http://www.w3.org/2000/svg" height="140" width="300" style="user-select: none;">
    <g id="lcd2" transform="scale(1.4 1.4)" fill="#5f9ea0"></g>
    <path d="M230 10 l30 30 h-60 z" fill="#5f9ea0" onclick="up()"/>
    <path d="M230 85 l-30 -30 h60 z" fill="#5f9ea0" onclick="down()"/>
</svg>
<div>
  Tolerance: <input type="number" value="%THRESHOLD%" step=".1" min="0" onchange="updTreshold(this.value)"
  style="width: 50px; border: none"/>
</div>

<script>
    let dMap = [
        [1, 2, 3, 4, 5, 6], // 0
        [2, 3],             // 1
        [1, 2, 4, 5, 7],    // 2
        [1, 2, 3, 4, 7],    // 3
        [2, 3, 6, 7],       // 4
        [1, 3, 4, 6, 7],    // 5
        [1, 3, 4, 5, 6, 7], // 6
        [1, 2, 3],          // 7
        [1, 2, 3, 4, 5, 6, 7], // 8
        [1, 2, 3, 4, 6, 7]  // 9
    ]
    digit = d => {
        let paths = [
            'M2.1 2L.2 0h31.6l-1.9 2L28 4H4z',
            'M30 30l-2-1V5l2-1.9 2-2V31l-2-1z',
            'M30 60.9l-2-2V35l2-.9 2-1V62.9z',
            'M1.5 62.6l1.9-2 .6-.5h24l1.4 1.4 1.9 1.9.5.5H.2z',
            'M0 48L.2 33l2 1 1.8 1v24l-2 1.8-1.8 2z',
            'M0 16V1.2L2 3l1.9 2V29l-2 1-1.8.9z',
            'M16 33.9H3.9l-1.9-1L.2 32l.2-.1 1.8-1 1.7-.8H28l1.9 1 1.8 1-1.9.9-1.9 1z',

        ]
        return paths.map((p, i) => `<path d="${p}"
            opacity="${dMap[d].indexOf(i + 1) == -1 ? .05 : 1}"/>`).join('\n')
    }

    let drawNumber = (n , id) => {

        const [d0, d1, ,d2] = ('00' + n.toFixed(1)).slice(-4)
        document.getElementById(id).innerHTML =
        '<g transform="translate(10, 0) skewX(-5)">' + digit(d0) + '</g>' +
        '<g transform="translate(50 0) skewX(-5)">' + digit(d1) + '</g>' +
        '<rect transform=" skewX(-5)" x="88" y="59" width="5" height="5" />' +
        '<g transform="translate(100 0) skewX(-5)">' + digit(d2) + '</g>'
    }
    let crtTemp = Number(%TEMPERATURE%)
    let destTemp = %DEST_TEMP%
    drawNumber(crtTemp, 'lcd1')
    drawNumber(destTemp, 'lcd2')
    const up = () => {
        destTemp += .5
        upd()
    }
    const down = () => {
        destTemp -= .5
        upd()
    }
    const upd = () => (drawNumber(destTemp, 'lcd2'), fetch('/get?dest_temp=' + destTemp))
    const updTreshold = (value) => fetch('/get?treshold=' + value)
    
</script>

</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request)
{
	request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

// Replaces placeholder with values in HTML
String processor(const String &var)
{
	//Serial.println(var);
	if (var == "TEMPERATURE")
	{
		return lastTemperature;
	}
	else if (var == "DEST_TEMP")
	{
		return destTemp;
	}
	else if (var == "THRESHOLD")
	{
		return treshold;
	}
	return String();
}

// Flag variable to keep track if triggers was activated or not
bool triggerActive = false;

// Interval between sensor readings
unsigned long previousMillis = 0;
const long interval = 5000;

// GPIO where the output is connected to
const int output = 2;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("WiFi Failed!");
		return;
	}
	Serial.println();
	Serial.print("ESP IP Address: http://");
	Serial.println(WiFi.localIP());

	pinMode(output, OUTPUT);
	digitalWrite(output, LOW);

	// Start the DS18B20 sensor
	sensors.begin();

	// Send web page to client
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send_P(200, "text/html", index_html, processor);
	});

	server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (request->hasParam("dest_temp")) {
			destTemp = request->getParam("dest_temp")->value();
		}
		if (request->hasParam("treshold")) {
			treshold = request->getParam("treshold")->value();
		}
		Serial.println(destTemp);
		request->send(202, "text/html", "");
	});
	server.onNotFound(notFound);
	server.begin();
}

void loop()
{
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= interval) {
		previousMillis = currentMillis;
		sensors.requestTemperatures();
		// Temperature in Celsius
		float temperature = sensors.getTempCByIndex(0);
		Serial.print(temperature);
		Serial.println(" *C");

		lastTemperature = String(temperature);

		const float th = treshold.toFloat();
		const float dt = destTemp.toFloat();

		// Check if temperature is above threshold and if it needs to trigger output
		if (temperature > dt && !triggerActive) {
			String message = String("Temperature above threshold. Current temperature: ") +
							 String(temperature) + String("C");
			Serial.println(message);
			triggerActive = true;
			digitalWrite(output, HIGH);
		}
		// Check if temperature is below threshold and if it needs to trigger output
		else if ((temperature < dt - th) && triggerActive) {
			String message = String("Temperature below threshold. Current temperature: ") +
							 String(temperature) + String(" C");
			Serial.println(message);
			triggerActive = false;
			digitalWrite(output, LOW);
		}
	}
}
