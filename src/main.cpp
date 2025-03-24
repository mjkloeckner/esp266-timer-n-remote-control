#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

uint8_t remote_devices = 0, main_output_enabled = 0;
uint32_t t = 0, dt = 0, main_output_dt = 0;

ESP8266WebServer server(80);
WebSocketsServer web_socket = WebSocketsServer(81);
JSONVar data, system_time_data;
WiFiUDP ntpUDP;

NTPClient system_time(ntpUDP, "pool.ntp.org", 0, 3600); // UTC Time

#define REMOTE_LED_PIN LED_BUILTIN
#define MAIN_OUTPUT_PIN 5
#define MAIN_SWITCH_INPUT_PIN 4
#define SETUP_COMPLETE_OUTPUT_PIN 14

#define WIFI_SSID "hello-world"
#define WIFI_PASSWD "12345678"
#define MDNS_DOMAIN "esp8266"

#define MAIN_SWITCH_REPEAT_TIME 200

void webp_socket_event(uint8_t, WStype_t, uint8_t *, size_t);
void webp_handler();

void main_output_toggle();
void update_all_socket_clients();

void setup_wifi();
void setup_websocket();
void setup_webserver();
void setup_mdns();
void setup_fs();

void setup() {
    pinMode(REMOTE_LED_PIN, OUTPUT);
    pinMode(MAIN_OUTPUT_PIN, OUTPUT);
    pinMode(MAIN_SWITCH_INPUT_PIN, INPUT);
    pinMode(SETUP_COMPLETE_OUTPUT_PIN, OUTPUT);

    digitalWrite(REMOTE_LED_PIN, HIGH); // led builtin uses inverted logic
    digitalWrite(MAIN_OUTPUT_PIN, LOW);
    digitalWrite(SETUP_COMPLETE_OUTPUT_PIN, LOW);

    // Serial.begin(921600);
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    Serial.printf("\n\n\n");
    delay(1000);

    Serial.printf("[SETUP] Booting");
    for(uint8_t t = 20; t > 0; t--) {
        Serial.print(".");
        Serial.flush();
        delay(150);
    }
    Serial.print("\n");

    setup_wifi();
    setup_fs();
    setup_websocket();
    setup_mdns();
    setup_webserver();
    system_time.begin();
    digitalWrite(SETUP_COMPLETE_OUTPUT_PIN, HIGH);
}

void loop() {
    t = millis();

    if(digitalRead(MAIN_SWITCH_INPUT_PIN)) {
        if((t - dt) > MAIN_SWITCH_REPEAT_TIME) {
            dt = millis();
            main_output_toggle();
            update_all_socket_clients();
        }
    }

    // if((t - dt) > MAIN_OUTPUT_PERIOD) {
    //     dt = millis();
    //     digitalWrite(MAIN_OUTPUT_PIN, HIGH);
    //     main_output_dt = millis();
    // }

    // if((t - main_output_dt) > MAIN_OUTPUT_ON_TIME) {
    //     digitalWrite(MAIN_OUTPUT_PIN, LOW);
    // }

    MDNS.update();
    web_socket.loop();
    server.handleClient();
    system_time.update();
}

void setup_wifi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    Serial.printf("[SETUP] Connecting to WiFi");
    while(WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }
    Serial.printf("\n[SETUP] Connected to '%s' IP address ", WIFI_SSID);
    Serial.println(WiFi.localIP());
}

void setup_mdns() {
    if (!MDNS.begin(MDNS_DOMAIN)) {
        Serial.println("[SETUP] Error setting up MDNS responder!");
        while(1) { delay(100); }
    }

    MDNS.addService("http", "tcp", 80);
    Serial.printf("[SETUP] mDNS started domain '%s.local'\n", MDNS_DOMAIN);
}

void main_output_toggle() {
    if(main_output_enabled) {
        // GPOC = (1 << MAIN_OUTPUT_PIN); // low
        digitalWrite(MAIN_OUTPUT_PIN, LOW);
    } else {
        // GPOS = (1 << MAIN_OUTPUT_PIN);  // high
        digitalWrite(MAIN_OUTPUT_PIN, HIGH);
    }
    main_output_enabled = !main_output_enabled;
}

void query_system_time() {
    system_time_data["system-time"] = system_time.getEpochTime();
}

void update_data() {
    data["main-output-enabled"] = String(main_output_enabled);
    data["system-local-domain"] = String(MDNS_DOMAIN);
    data["system-ip-addr"] = WiFi.localIP().toString();
    data["system-time"] = system_time.getEpochTime();
    data["wifi-ssid"] = WIFI_SSID;
    data["wifi-rssi"] = WiFi.RSSI();
}

void update_all_socket_clients() {
    update_data();
    String data_as_json = JSON.stringify(data);
    web_socket.broadcastTXT(data_as_json);
}

void webp_socket_event(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
    switch(type) {
    case WStype_DISCONNECTED:
        Serial.printf("[SOCKET] %d: Disconnected\n", num);
        remote_devices -= 1;
        digitalWrite(REMOTE_LED_PIN, remote_devices ? LOW : HIGH);
        break;
    case WStype_CONNECTED: {
            IPAddress ip = web_socket.remoteIP(num);
            Serial.printf("[SOCKET] %u: Connected from %d.%d.%d.%d URL %s\n",
                    num, ip[0], ip[1], ip[2], ip[3], payload);
            remote_devices += 1;
            digitalWrite(REMOTE_LED_PIN, remote_devices ? LOW : HIGH);
        }
        break;
    case WStype_TEXT: {
            // Serial.printf("[SOCKET](%u) got text: %s\n", num, payload);

            if(!strcmp((char *)payload, "main-output-toggle")) {
                main_output_toggle();
                update_all_socket_clients();
            }

            if(!strcmp((char *)payload, "main-output-status")) {
                update_data();
                String data_as_json = JSON.stringify(data);
                web_socket.sendTXT(num, data_as_json);
            }

            if(!strcmp((char *)payload, "system-time")) {
                String system_time_data_as_json = JSON.stringify(system_time_data);
                web_socket.sendTXT(num, system_time_data_as_json);
            }
        }
        break;
    case WStype_BIN:
        Serial.printf("[SOCKET][%u] get binary length: %u\n", num, len);
        hexdump(payload, len);
        // web_socket.sendBIN(num, payload, len);
        break;
    }
}

void setup_websocket() {
    web_socket.begin();
    web_socket.onEvent(webp_socket_event);
}

String webserver_get_file(String path) {
    String return_page;

    if(LittleFS.exists(path)) {
        Serial.printf("[SERVER] Serving file '%s'\n", path.c_str());
        File file = LittleFS.open(path.c_str(), "r");
        while(file.available()) {
            return_page += (char)file.read();
        }
        file.close();
    } else {
        Serial.printf("[SERVER] '%s' File Not Found\n", path.c_str());
        return_page = R"==(<!DOCTYPE html>
        <html>
          <head>
              <title>ERROR 404: File Not found!!</title>
              <meta name="viewport" content="width=device-width, initial-scale=1.0">
          </head>
          <body>
              <h>ERROR 404: File Not Found!</h1>
              <p>file '/index.html' not found</p>
          </body>
        </html>)==";
    }
    return return_page;
}

String webserver_file_content_type(String path) {
  if (path.endsWith(".html")) return "text/html";
  else if (path.endsWith(".css")) return "text/css";
  else if (path.endsWith(".js")) return "application/javascript";
  else if (path.endsWith(".ico")) return "image/x-icon";
  else if (path.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void webserver_file_handler() {
    String path = server.uri();
    server.send(200, webserver_file_content_type(path), webserver_get_file(path));
}

void webserver_handle_root() {
    String webp_index = webserver_get_file("index.html");
    server.send(200, "text/html", webp_index.c_str());
}

void setup_webserver() {
    Serial.println("[SETUP] loading server response from file 'index.html'");

    server.on("/", webserver_handle_root);
    server.onNotFound(webserver_file_handler);

    server.begin();
}

void setup_fs() {
    if(LittleFS.begin() == 0) {
        Serial.println("[SETUP] Error couldn't begin filesystem!");
    }

    FSInfo fs_info;
    LittleFS.info(fs_info);

    uint8_t percentage_usad = (fs_info.usedBytes/fs_info.totalBytes)*100;
    Serial.printf("[SETUP] LittleFS started: spaced used %d%%\n", percentage_usad);
}
