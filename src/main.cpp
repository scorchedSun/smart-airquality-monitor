#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cctype>
#include <functional>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "WebServer.h"
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "Adafruit_SSD1306.h"
#include "ArduinoJson.h"
#include "PubSubClient.h"
#include "WiFi.h"
#include "ConfigAssist.h"
#include "ConfigAssistHelper.h"
#include "driver/ledc.h"

#include "user-variables.h"
#include "ReconnectingPubSubClient.h"
#include "ValueReporter.h"
#include "Measurement.h"
#include "HomeAssistantDevice.h"
#include "HomeAssistantSensorReporter.h"
#include "Logger.h"
#include "Fan.h"
#include "DHT20Wrapper.h"
#include "MHZ19Wrapper.h"
#include "PMWrapper.h"
#include "Translator.h"

// OLED -> GND | VCC | SCK | SDA | RES | DC | CS
#define OLED_MOSI   23  // SDA
#define OLED_CLK    18  // SCK
#define OLED_DC     16
#define OLED_CS     5
#define OLED_RESET  17

#define MHZ19_BAUD_RATE 9600
#define MHZ19_RX_ATTACHED_TO 33
#define MHZ19_TX_ATTACHED_TO 25

#define PMS_RX_ATTACHED_TO 26
#define PMS_TX_ATTACHED_TO 27

#define FAN_PWM_PIN 13
#define FAN_FREQUENCY_HZ 25000

static const std::string app_version = "1.0.0";

static const uint16_t wifi_configuration_timeout_in_seconds(300);
static const uint16_t sensor_warmup_millis(2000);
static const std::string co2_unit("ppm");
static const std::string temperature_unit("C");
static const std::string humidity_unit("%");
static const std::string pms_unit("ug/m3");
static const std::string device_prefix("smaq_");

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

Fan fan(FAN_PWM_PIN, FAN_FREQUENCY_HZ);

time_t data_last_read_timestamp;
uint32_t report_interval_in_seconds(300);   // 5 minutes
uint32_t display_each_measurement_for_in_millis(10000);  // 10 seconds

std::shared_ptr<HomeAssistantDevice> home_assistant_device;
std::shared_ptr<ReconnectingPubSubClient> reconnecting_mqtt_client;

ConfigAssist config(CA_DEF_CONF_FILE, VARIABLES_DEF_YAML);
WebServer server(80);

std::vector<std::unique_ptr<ValueReporter>> measurement_reporters;
std::vector<char> wait_animation_chars { '-', '\\', '|', '/' };
std::vector<std::unique_ptr<Measurement>> measurements;

IPAddress ip_address;
std::string mac_id;

std::string mqtt_broker;
std::string mqtt_client_id;

IPAddress syslog_server_ip;
std::uint16_t syslog_server_port;

std::string log_level;

DHT20Wrapper dht20;
MHZ19Wrapper mhz19(MHZ19_RX_ATTACHED_TO, MHZ19_TX_ATTACHED_TO, MHZ19_BAUD_RATE);
PMWrapper pms(PMS5003, PMS_TX_ATTACHED_TO, PMS_RX_ATTACHED_TO);
DisplayUnitTranslator display_unit_translator;
FriendlyNameTypeTranslator friendly_name_type_translator;

bool should_save_config(false);
bool is_setup(false);
bool mqtt_configured(false);
bool discovery_messages_sent(false);
bool web_portal_requested(false);
bool display_enabled(true);
bool display_setup(false);
bool syslog_server_enabled(false);

void add_details_on_display() {
  if (!display_enabled || !display_setup) return;

  if (!ip_address.toString().isEmpty()) {
    display.setCursor(0,0);
    display.print("IP: ");
    display.print(ip_address);
  }
}

void show_on_display(const std::string& value) {
  if (!display_enabled || !display_setup) return;

  display.clearDisplay();
  add_details_on_display();
  display.setCursor(5,28);
  display.println(value.c_str());
  display.display();
}

void show_wait_animation(const std::string title, const uint32_t time_millis) {
  uint32_t start_time = millis();
  uint8_t animation_index = 0;
  uint8_t max_animation_index = (uint8_t)wait_animation_chars.size();

  while (millis() - start_time <= time_millis) {
    if (display_enabled && display_setup) {
      display.clearDisplay();
      add_details_on_display();
      display.setCursor(0,16);
      display.println(title.c_str());
      display.setCursor(60, 36);
      display.print(wait_animation_chars.at(animation_index));
      display.display();
      animation_index = ++animation_index % max_animation_index;
    }
    delay(100);
  }
}

void register_handlers() {
  server.on("/", []() {
    server.sendHeader("Location", "/cfg", true);
    server.send(301, "text/plain", "");
  });
  server.on("/cfg", []() {
    config.init();
    web_portal_requested = true;
  });
}

void setup_display() {
  if (display_setup) return;

  if (!display.begin(SSD1306_SWITCHCAPVCC))
  {
    LOG_E("Display could not be set up.");
    for (;;);
  }

  display_setup = true;

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  delay(100);
}

void turn_display_off() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void turn_display_on() {
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void read_device_configuration() {
  display_enabled = config.exists("enable_display") ? config["enable_display"].toInt() : true;
  report_interval_in_seconds = config["report_interval"].toInt() * 60;
  display_each_measurement_for_in_millis = config["display_interval"].toInt() * 1000;
}

void read_log_server_configuration() {
  if (config.exists("syslog_server_ip") && !config["syslog_server_ip"].isEmpty()) {
    syslog_server_ip.fromString(config["syslog_server_ip"].c_str());
    syslog_server_port = config["syslog_server_port"].toInt();

    syslog_server_enabled = true;
  }

  log_level = std::string(config.exists("log_level") ? config["log_level"].c_str() : "Error");
}

void setup_mqtt(const std::string& mqtt_device_id, std::shared_ptr<HomeAssistantDevice> home_assistant_device) {
  mqtt_broker = std::string(config["mqtt_broker"].c_str());

  uint16_t mqtt_port(config["mqtt_port"].toInt());
  std::string mqtt_user(config["mqtt_user"].c_str());
  std::string mqtt_password(config["mqtt_pass"].c_str());

  mqtt_configured = !mqtt_broker.empty() && mqtt_port > 0;

  if (mqtt_configured && WiFi.isConnected()) {
    mqtt_client.setServer(mqtt_broker.c_str(), mqtt_port);
    mqtt_client.setBufferSize(512);
    reconnecting_mqtt_client = std::make_shared<ReconnectingPubSubClient>(std::make_shared<PubSubClient>(mqtt_client), mqtt_user, mqtt_password, mqtt_device_id);
    measurement_reporters.push_back(std::make_unique<HomeAssistantSensorReporter>(home_assistant_device, reconnecting_mqtt_client));
  }
}

void initialize_sensors() {
  pms.begin();  

  dht20.begin();  

  mhz19.begin();

  show_wait_animation("Initializing sensors", 120000);  // Wait 2 minutes
}

void setup() {
  Wire.begin();

  Serial.begin(115200);
  Serial.print("\n\n\n\n");
  Serial.flush();

  fan.begin(20);
  
  ConfigAssistHelper config_helper(config);
  bool wifi_connected = config_helper.connectToNetwork();

  config.setup(server, !wifi_connected);

  register_handlers();

  server.begin();

  if (!wifi_connected) {
    ip_address.fromString(WiFi.softAPIP().toString().c_str());
    std::string accesspoint_ssid(config.getHostName().c_str());
    std::string message("Connecting to WiFi failed. Configure via AP '" + accesspoint_ssid + "'");
    setup_display();
    show_on_display(message);
    return;
  }  

  ip_address.fromString(WiFi.localIP().toString().c_str());
  mac_id = config.getMacID().c_str();

  read_device_configuration();
  read_log_server_configuration();

  setup_display();

  if (display_enabled && display_setup) {
    show_on_display("Booting ...");
  } else {
    turn_display_off();
  }

  std::string friendly_name(config["friendly_name"].c_str());

  home_assistant_device = std::make_shared<HomeAssistantDevice>(device_prefix, mac_id, friendly_name, app_version);

  Logger::Level parsed_log_level;

  if (!Logger::try_get_level_by_name(log_level, parsed_log_level)) {    
    std::string message("Setup failed. Invalid configuration. Value '" + log_level + "' is not a supported log level.");
    show_on_display(message);
    return;
  }

  logger.setup_serial(Logger::Level::Debug);
  if (syslog_server_enabled) {
    logger.setup_syslog(syslog_server_ip, syslog_server_port, mac_id, Logger::Level::Debug);
  }

  setup_mqtt(home_assistant_device->get_device_id(), home_assistant_device);
  
  initialize_sensors();

  is_setup = true;
}

bool should_read_new_measurements() {
  time_t now;
  time(&now);
  return data_last_read_timestamp == 0 || now - data_last_read_timestamp >= report_interval_in_seconds;
}

void show_on_display(const std::unique_ptr<Measurement>& measurement) {
  std::string message(friendly_name_type_translator.translate(measurement->get_details().get_type()));
  message
    .append(": ")
    .append(measurement->value_to_string())
    .append(" ")
    .append(display_unit_translator.translate(measurement->get_details().get_unit()));
  show_on_display(message);
  delay(display_each_measurement_for_in_millis);
}

void loop() {
  server.handleClient();

  if (!web_portal_requested) {
    if (mqtt_configured) {
      mqtt_client.loop();
    }

    if (is_setup) {       
      if (should_read_new_measurements()) {
        measurements.clear();

        dht20.provide_measurements(measurements);
        mhz19.provide_measurements(measurements);
        pms.provide_measurements(measurements);

        for (const auto& reporter : measurement_reporters) {
          reporter->report(measurements);
        }

        time(&data_last_read_timestamp);
      }
      else {
        for (const auto& measurement : measurements) {
          show_on_display(measurement);    
        }
      }
    }
  }
}