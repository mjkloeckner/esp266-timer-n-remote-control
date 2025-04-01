#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>
#include <time.h>
#include <coredecls.h> // settimeofday_cb() callback

#define REMOTE_LED_PIN LED_BUILTIN
#define MAIN_OUTPUT_PIN 5
#define MAIN_SWITCH_INPUT_PIN 4

#define WIFI_SSID "hello-world"
#define WIFI_PASSWD "12345678"
#define MDNS_DOMAIN "esp8266"

#define MY_NTP_SERVER "pool.ntp.org"
#define MY_TZ "<-03>3"

#define MAIN_SWITCH_REPEAT_TIME 200

typedef struct {
    uint8_t from_hour, from_minute;
    uint8_t to_hour, to_minute;
} timer_values_t;

ESP8266WebServer server(80);
WebSocketsServer web_socket = WebSocketsServer(81);
JSONVar data, timer_data;
timer_values_t timer_values;

time_t system_time, last_ntp_sync;
tm tm;
uint8_t remote_devices = 0, timer_enabled = 0, main_output_enabled = 0;

void webp_socket_event(uint8_t, WStype_t, uint8_t *, size_t);
void webp_handler();

void main_output_toggle();
void update_all_socket_clients();

void setup_wifi();
void setup_websocket();
void setup_webserver();
void setup_mdns();
void setup_fs();
void update_timer_values(const timer_values_t);
void update_timer_data();
void update_all_clients_checkbox();

void save_timer_values_to_file() {
    const char* file_path = "timer_values.json";
    File file = LittleFS.open(file_path, "w");

    if (!file) {
        Serial.printf("[LITTLEFS] Failed to open file `%s` for writing\n", file_path);
        return;
    }

    update_timer_data();
    if (file.print(JSON.stringify(timer_data).c_str())) {
        Serial.printf("[LITTLEFS] writing `%s` to file\n", JSON.stringify(timer_data).c_str());
        Serial.println("[LITTLEFS] Timer values saved");
    } else {
        Serial.println("[LITTLEFS] Timer values write failed");
    }

    // delay(1000);  // Make sure the CREATE and LASTWRITE times are different
    file.close();
}

void read_timer_values_from_file() {
    const char* file_path = "timer_values.json";
    File file = LittleFS.open(file_path, "r");

    if (!file) {
        Serial.printf("[LITTLEFS] Failed to open file `%s`. Setting default values..\n", file_path);
        timer_values_t new_values = {18, 0, 0, 0};
        update_timer_values(new_values);
        return;
    }

    String timer_values_json = file.readString();
    file.close();

    Serial.printf("[LITTLEFS] Read `%s` from %s\n", timer_values_json.c_str(), file_path);

    JSONVar timer_values = JSON.parse(timer_values_json.c_str());

    if(JSON.typeof(timer_values) == "undefined") {
        Serial.println("[SOCKET] Parsing file `timer-values.json` failed");
        return;
    }

    timer_values_t new_values = {
        (uint8_t)String(timer_values["from"]["hour"]).toInt(),
        (uint8_t)String(timer_values["from"]["minute"]).toInt(),
        (uint8_t)String(timer_values["to"]["hour"]).toInt(),
        (uint8_t)String(timer_values["to"]["minute"]).toInt()
    };

    timer_enabled = String(timer_values["enabled"]).toInt();

    Serial.printf("[UPDATE_TIMER_VALUES] %02d:%02d to %02d:%02d\n",
            new_values.from_hour, new_values.from_minute,
            new_values.to_hour, new_values.to_minute);

    update_timer_values(new_values);
    update_timer_data();
}

void show_time(bool from_sntp = false) {
    time(&system_time);              // read the current time
    localtime_r(&system_time, &tm);  // update the structure tm with the current time

    const char* prompt = from_sntp ? "[SNTP]" : "[LOOP]";

    if(from_sntp) {
        time(&last_ntp_sync);
    }

    // YYYY-MM-DD HH:MM:SS GMT-3
    Serial.printf("%s %d-%02d-%02d %02d:%02d:%02d GMT-3\n", prompt,
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// ntp startup delay
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 () {
    randomSeed(A0);
    return random(5000);
}

// ntp polling interval
uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 () {
    return 8 * 60 * 60 * 1000UL; // 8*60 mins
}

void setup() {
    pinMode(REMOTE_LED_PIN, OUTPUT);
    pinMode(MAIN_OUTPUT_PIN, OUTPUT);
    pinMode(MAIN_SWITCH_INPUT_PIN, INPUT);

    digitalWrite(REMOTE_LED_PIN, HIGH); // led builtin uses inverted logic
    digitalWrite(MAIN_OUTPUT_PIN, LOW);

    Serial.begin(115200);
    Serial.setDebugOutput(false);
    Serial.printf("\n\n\n");
    delay(1000);

    Serial.printf("[SETUP] Booting");
    for(uint8_t t = 10; t > 0; t--) {
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
    configTime(MY_TZ, MY_NTP_SERVER); // Here is the IMPORTANT ONE LINER needed in your sketch!
    settimeofday_cb(show_time);       // ntp update callback
    show_time();
    read_timer_values_from_file();
}

uint32_t t = 0, timer_dt = 0, clock_dt = 0, main_output_dt = 0;

void update_timer_output() {
    // check timer output every minute
    if((t - clock_dt) > 60*1000) {
        localtime_r(&system_time, &tm);  // update the structure tm with the current time
        if((timer_enabled) && (!main_output_enabled)) {
            if((tm.tm_hour >= timer_values.from_hour)
                && (tm.tm_min >= timer_values.from_minute)
                && (tm.tm_min <= timer_values.to_hour)
                && (tm.tm_min <= timer_values.to_minute)) {

                digitalWrite(MAIN_OUTPUT_PIN, HIGH);
                main_output_enabled = 1;
                update_all_clients_checkbox();
            } 
            else {
                digitalWrite(MAIN_OUTPUT_PIN, LOW);
                main_output_enabled = 0;
                update_all_clients_checkbox();
            }
        }
    }
}

void loop() {
    t = millis();

    if((t - main_output_dt) > MAIN_SWITCH_REPEAT_TIME) {
        if(digitalRead(MAIN_SWITCH_INPUT_PIN)) {
            main_output_dt = millis();
            main_output_toggle();
            update_all_clients_checkbox();
        }
    }

    MDNS.update();
    web_socket.loop();
    server.handleClient();

    // update `system_time` every 5s
    if((t - clock_dt) > 5000){
        clock_dt = millis();
        time(&system_time); // read the current time
    }

    update_timer_output();
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

void timer_toggle() {
    timer_enabled = !timer_enabled;
}

void clear_data() {
    JSONVar keys = data.keys();
    for (int i = 0; i < keys.length(); i++) {
        data[keys[i]] = undefined;
    }
}

void update_data() {
    data["main-output-enabled"] = String(main_output_enabled);
    data["system-local-domain"] = String(MDNS_DOMAIN);
    data["system-ip-addr"]      = WiFi.localIP().toString();
    data["last-ntp-sync"]       = (long)last_ntp_sync;
    data["system-time"]         = (long)system_time;
    data["wifi-ssid"]           = WIFI_SSID;
    data["wifi-rssi"]           = WiFi.RSSI();

    update_timer_data();
    data["timer"] = timer_data;
}

void update_all_socket_clients() {
    update_data();
    String data_as_json = JSON.stringify(data);
    web_socket.broadcastTXT(data_as_json);
}

void update_all_clients_checkbox() {
    JSONVar output_data;
    output_data["type"] = "cb";
    output_data["main-output-enabled"] = String(main_output_enabled);
    output_data["timer"]["enabled"] = String(timer_enabled);
    String output_data_as_json = JSON.stringify(output_data);
    web_socket.broadcastTXT(output_data_as_json);
}

String left_pad(uint8_t n) {
    return n < 10 ? "0" + String(n) : String(n);
}

void update_timer_data() {
    timer_data["enabled"]        = String(timer_enabled);
    timer_data["from"]["hour"]   = left_pad(timer_values.from_hour);
    timer_data["from"]["minute"] = left_pad(timer_values.from_minute);
    timer_data["to"]["hour"]     = left_pad(timer_values.to_hour);
    timer_data["to"]["minute"]   = left_pad(timer_values.to_minute);
}

void update_all_clients_timer_values() {
    clear_data();
    update_timer_data();
    data["type"] = "timer";
    data["timer"] = timer_data;
    web_socket.broadcastTXT(JSON.stringify(data).c_str());
}

void update_timer_values(const timer_values_t new_values) {
    timer_values = new_values;
    Serial.printf("[UPDATE_TIMER_VALUES] %02d:%02d to %02d:%02d\n",
            timer_values.from_hour, timer_values.from_minute,
            timer_values.to_hour, timer_values.to_minute);
}

typedef enum {
    MAIN_OUTPUT_STATUS = 0,
    MAIN_OUTPUT_TOGGLE,
    TIMER_TOGGLE,
    TIMER_SET_VALUES
} query_t;

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
            int query_type = *payload - '0';

            switch(query_type) {
            case MAIN_OUTPUT_TOGGLE:
                main_output_toggle();
                update_all_clients_checkbox();
                break;
            case TIMER_TOGGLE:
                timer_toggle();
                update_all_clients_checkbox();
                break;
            case TIMER_SET_VALUES: {
                    payload++; // skip query type
                    Serial.printf("[SOCKET] %s\n", payload);
                    timer_data = JSON.parse((char *)payload);

                    if(JSON.typeof(timer_data) == "undefined") {
                        Serial.println("[SOCKET] Parsing payload failed!");
                        break;
                    }

                    timer_values_t new_values = {
                        (uint8_t)String(timer_data["from"]["hour"]).toInt(),
                        (uint8_t)String(timer_data["from"]["minute"]).toInt(),
                        (uint8_t)String(timer_data["to"]["hour"]).toInt(),
                        (uint8_t)String(timer_data["to"]["minute"]).toInt()
                    };

                    update_timer_values(new_values);
                    save_timer_values_to_file();
                    update_all_clients_timer_values();
                }
                break;
            case MAIN_OUTPUT_STATUS:
            default:
                update_data();
                data["type"] = "all";
                String data_as_json = JSON.stringify(data);
                web_socket.sendTXT(num, data_as_json);
                break;
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

int webserver_get_file(String path, String &return_page) {
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
        return 1;
    }
    return 0;
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
    String requested_page;
    int response_code;
    response_code = webserver_get_file(path, requested_page) ? 404 : 200;
    server.send(response_code, webserver_file_content_type(path), requested_page);
}

void webserver_handle_root() {
    String index_page;
    int response_code = 200;
    if(webserver_get_file("index.html", index_page)) {
        response_code = 404;
    }
    server.send(response_code, "text/html", index_page.c_str());
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
