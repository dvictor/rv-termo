# rv-termo
esp8266 thermostat 

The quick and dirty thermostat that I use in my RV. 
Connects to the local wifi and can display a simple HTML interface.

WiFi credentials file, not checked-in: `wifi-secrets.h`

```
#if !defined(WIFI_SECRETS_H)
#define WIFI_SECRETS_H 1

const char* ssid = "...";
const char* password = "...";

#endif
```
