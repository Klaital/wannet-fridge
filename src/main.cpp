#include <Arduino.h>
#include <HttpClient.h>
#include <influxdb.h>
#include <WiFiNINA.h>
#include <SPI.h>
#include "Adafruit_MAX31855.h"

#include "secrets.h"

// #define DEBUG 1

// WiFi setup
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;
int wifiStatus = WL_IDLE_STATUS;
WiFiClient client;

// Thermocouple setup
#define THERMO1_DO 2
#define THERMO1_CS 3
#define THERMO1_CLK 4
Adafruit_MAX31855 thermo1(THERMO1_CLK, THERMO1_CS, THERMO1_DO);

// InfluxDB setup
HttpClient influx_client(INFLUX_HOST, INFLUX_PORT, &client);
Influx::Point fridge_data;
Influx::Point freezer_data;
HTTP::Request influx_req;

void connect_WiFi() {
    // attempt to connect to Wifi network:
    while (wifiStatus != WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        wifiStatus = WiFi.begin(ssid, pass);
        Serial.print("WiFi Status: ");
        Serial.println(wifiStatus);
        // wait 10 seconds for connection:
        delay(5000);
    }
}

void printWifiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    const long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.print("IP: ");
    Serial.println(ip);
}

void print_request(HTTP::Request& req);

void setup() {
    Serial.begin(9600);
#ifdef DEBUG
    while(!Serial)
        ; //no-op, just wait here until someone connects to the serial port
#endif

    // Set up networking
    connect_WiFi();
    printWifiStatus();

    // Configure influx clients
    fridge_data.set_measurement("fridge");
    fridge_data.set_tag("room", "kitchen");
    strcpy(influx_req.method, "POST");
    sprintf(influx_req.path, "/api/v2/write?org=%s&bucket=%s&precision=s", INFLUX_ORG, INFLUX_BUCKET);
    influx_req.headers.set("Content-Type", "text/plain; charset=utf-8");
    influx_req.headers.set("Accept", "application/json");
    influx_req.headers.set("Authorization", INFLUX_AUTHORIZATION);



    // Set up sensors
    delay(500);
    Serial.println("Initializing thermocouples...");
    if (!thermo1.begin()) {
        Serial.println("Error connecting to thermo1");
        while(1) delay(10); // halt and catch fire
    }
    Serial.println("Ready!");


}
void loop() {
    // check the thermocouple
#ifdef DEBUG
    Serial.print("Internal=");
    Serial.print(thermo1.readInternal());
    Serial.print(" ");
#endif
    const double t1 = thermo1.readFahrenheit();
    if (isnan(t1)) {
        Serial.println("Thermo1 fault detected!");
        const uint8_t e = thermo1.readError();
        if (e & MAX31855_FAULT_OPEN) Serial.println("FAULT: Thermocouple is open - no connections.");
        if (e & MAX31855_FAULT_SHORT_GND) Serial.println("FAULT: Thermocouple is short-circuited to GND.");
        if (e & MAX31855_FAULT_SHORT_VCC) Serial.println("FAULT: Thermocouple is short-circuited to VCC.");
    } else {
        Serial.print(" probe=");
        Serial.println(t1);
    }

    // send the readings to influxdb
    fridge_data.set_field("temp", t1);
    fridge_data.timestamp = WiFi.getTime();
    if (fridge_data.timestamp > 0) {
        influx_req.body[0] = '\0';
        fridge_data.cat(influx_req.body);
        HTTP::Response resp;
        Serial.println("Sending data to influx");
        print_request(influx_req);
        influx_client.exec(influx_req, resp);
        Serial.print("Influx resp: ");
        Serial.println(resp.code);
    } else {
        Serial.println("Invalid timestamp");
    }

    // chill for a bit
    delay(5000);
}

void print_request(HTTP::Request &req) {
    char buf[512] = "";
    req.to_string(buf, 1024);
    Serial.println(buf);
}
