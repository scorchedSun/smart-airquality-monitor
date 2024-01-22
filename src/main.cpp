#include <FS.h>

#include <SPIFFS.h>

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
#include "MHZ19.h"
#include "SoftwareSerial.h"
#include "PMserial.h"
#include "DHT20.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "ArduinoJson.h"
#include "PubSubClient.h"
#include "ConfigAssist.h"
#include "ConfigAssistHelper.h"

#include "user-variables.h"
#include "ReconnectingPubSubClient.h"
#include "ValueReporter.h"
#include "SerialReporter.h"
#include "MQTTReporter.h"
#include "Measurement.h"
#include "HomeAssistantDevice.h"

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

static const std::string app_version = "1.0.0";

static const std::string ap_password("12345");
static const uint16_t wifi_configuration_timeout_in_seconds(300);
static const uint16_t sensor_warmup_millis(2000);
static const std::string co2_unit("ppm");
static const std::string temperature_unit("C");
static const std::string humidity_unit("%");
static const std::string pms_unit("ug/m3");
static const std::string device_prefix("smaq_");

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

MHZ19 mhz19;
SoftwareSerial mhz19_serial(MHZ19_TX_ATTACHED_TO, MHZ19_RX_ATTACHED_TO);

SerialPM pms(PMS5003, PMS_TX_ATTACHED_TO, PMS_RX_ATTACHED_TO); 

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

DHT20 dht20;

uint32_t data_last_read_timestamp(0);
uint32_t report_interval_in_millis(300000);   // 5 minutes
uint32_t display_each_measurement_for_in_millis(10000);   // 10 seconds

std::unique_ptr<HomeAssistantDevice> home_assistant_device;
std::shared_ptr<ReconnectingPubSubClient> reconnecting_mqtt_client;

ConfigAssist config(CA_DEF_CONF_FILE, VARIABLES_DEF_YAML);
WebServer server(80);

std::vector<std::unique_ptr<ValueReporter>> measurement_reporters;
std::vector<char> wait_animation_chars { '-', '\\', '|', '/' };
std::vector<std::unique_ptr<Measurement>> measurements;

std::string ip_address;
std::string mac_id;

std::string mqtt_broker;
std::string mqtt_client_id;

bool should_save_config(false);
bool is_setup(false);
bool mqtt_configured(false);
bool discovery_messages_sent(false);
bool web_portal_requested(false);
bool display_enabled(true);
bool display_setup(false);

const std::string device_classes_of_measurement_types[6] = {"temperature", "humidity", "pm1", "pm25", "pm10", "carbon_dioxide"};

void add_details_on_display() {
  if (!display_enabled || !display_setup) return;

  if (!ip_address.empty()) {
    display.setCursor(0,0);
    display.print("IP: ");
    display.print(ip_address.c_str());
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
    config.init();
    web_portal_requested = true;
    Serial.println("Web portal requested!");
    server.sendHeader("Location", "/cfg", true);
    server.send(301, "text/plain", "");
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
  report_interval_in_millis = config["report_interval"].toInt() * 60 * 1000;
  display_each_measurement_for_in_millis = config["display_interval"].toInt() * 1000;
}

void setup_mqtt(const std::string& mqtt_device_id, const std::string& sensor_state_topic) {
  mqtt_broker = std::string(config["mqtt_broker"].c_str());

  uint16_t mqtt_port(config["mqtt_port"].toInt());
  std::string mqtt_user(config["mqtt_user"].c_str());
  std::string mqtt_password(config["mqtt_pass"].c_str());

  mqtt_configured = !mqtt_broker.empty() && mqtt_port > 0;

  if (mqtt_configured && WiFi.isConnected()) {
    mqtt_client.setServer(mqtt_broker.c_str(), mqtt_port);
    mqtt_client.setBufferSize(512);
    reconnecting_mqtt_client = std::move(std::unique_ptr<ReconnectingPubSubClient>(new ReconnectingPubSubClient(std::make_shared<PubSubClient>(mqtt_client), mqtt_user, mqtt_password, mqtt_device_id)));
    measurement_reporters.push_back(std::unique_ptr<ValueReporter>(new MQTTReporter(reconnecting_mqtt_client, sensor_state_topic)));
  }
}

void initialize_sensors() {
  mhz19_serial.begin(MHZ19_BAUD_RATE);
  mhz19.begin(mhz19_serial); 
  mhz19.autoCalibration(false);   

  pms.init();
  pms.sleep();

  dht20.begin();  

  show_wait_animation("Initializing sensors", 120000);  // Wait 2 minutes
}

void setup() {
  Wire.begin();

  Serial.begin(115200);
  Serial.print("\n\n\n\n");
  Serial.flush();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  if (!SPIFFS.begin(true)) {
    LOG_E("Filesystem could not be mounted.");
    for (;;);
  }
  
  ConfigAssistHelper config_helper(config);
  bool wifi_connected = config_helper.connectToNetwork();

  config.setup(server, !wifi_connected);

  register_handlers();

  server.begin();

  if (!wifi_connected) {
    ip_address = WiFi.softAPIP().toString().c_str();
    std::string message("Connecting to WiFi failed. Configure via AP '%s'");
    std::snprintf(nullptr, 0, message.c_str(), WiFi.softAPSSID().c_str());
    setup_display();
    show_on_display(message);
    return;
  }  

  ip_address = std::string(WiFi.localIP().toString().c_str());
  mac_id = config.getMacID().c_str();

  read_device_configuration();

  setup_display();

  if (display_enabled && display_setup) {
    show_on_display("Booting ...");
  } else {
    turn_display_off();
  }

  std::string friendly_name(config["friendly_name"].c_str());

  home_assistant_device = std::unique_ptr<HomeAssistantDevice>(new HomeAssistantDevice(device_prefix, mac_id, friendly_name, app_version));

  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("Temperature", "temp", "temperature", "°C")));
  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("Humidity", "hum", "humidity", "%")));
  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("PM1", "pm1", "pm1", "µg/m³")));
  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("PM2.5", "pm25", "pm25", "µg/m³")));
  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("PM10", "pm10", "pm10", "µg/m³")));
  home_assistant_device->register_sensor(std::unique_ptr<HomeAssistantSensorDefinition>(new HomeAssistantSensorDefinition("CO2", "co2", "carbon_dioxide", "ppm")));

  setup_mqtt(home_assistant_device->get_device_id(), home_assistant_device->get_sensor_state_topic());

  measurement_reporters.push_back(std::unique_ptr<ValueReporter>(new SerialReporter(friendly_name)));
  
  initialize_sensors();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //enable brownout detector

  is_setup = true;
}

bool should_read_new_measurements() {
  return data_last_read_timestamp == 0 || millis() - data_last_read_timestamp >= report_interval_in_millis;
}

void report(const JsonDocument& measurements) {
  for (auto const& reporter : measurement_reporters) {
    reporter->report(measurements);
  }
}

void read_co2() {
  const uint32_t co2 = mhz19.getCO2(false);
  
  if (co2 > 0) {
    measurements.push_back(std::unique_ptr<Measurement>(new RoundNumberMeasurement(Measurement::Type::CO2, co2, co2_unit)));
  }
}

void read_temperature_and_humidity() {
  int status = dht20.read();

  switch (status)
    {
      case DHT20_OK:
        measurements.push_back(std::unique_ptr<Measurement>(new DecimalMeasurement(Measurement::Type::Humidity, dht20.getHumidity(), humidity_unit)));
        measurements.push_back(std::unique_ptr<Measurement>(new DecimalMeasurement(Measurement::Type::Temperature, dht20.getTemperature(), temperature_unit)));
        break;
      case DHT20_ERROR_CHECKSUM:
        Serial.print("Checksum error");
        break;
      case DHT20_ERROR_CONNECT:
        Serial.print("Connect error");
        break;
      case DHT20_MISSING_BYTES:
        Serial.print("Missing bytes");
        break;
      case DHT20_ERROR_BYTES_ALL_ZERO:
        Serial.print("All bytes read zero");
        break;
      case DHT20_ERROR_READ_TIMEOUT:
        Serial.print("Read time out");
        break;
      case DHT20_ERROR_LASTREAD:
        Serial.print("Error read too fast");
        break;
      default:
        Serial.print("Unknown error");
        break;
    }
}

void read_pms() {
  pms.wake();
  pms.read();
  if (pms)
  { // successfull read
    measurements.push_back(std::unique_ptr<Measurement>(new DecimalMeasurement(Measurement::Type::PM1, pms.pm01, pms_unit)));
    measurements.push_back(std::unique_ptr<Measurement>(new DecimalMeasurement(Measurement::Type::PM25, pms.pm25, pms_unit)));
    measurements.push_back(std::unique_ptr<Measurement>(new DecimalMeasurement(Measurement::Type::PM10, pms.pm10, pms_unit)));
  }
  else
  { // something went wrong
    switch (pms.status)
    {
    case pms.OK: // should never come here
      break;     // included to compile without warnings
    case pms.ERROR_TIMEOUT:
      Serial.println(F(PMS_ERROR_TIMEOUT));
      break;
    case pms.ERROR_MSG_UNKNOWN:
      Serial.println(F(PMS_ERROR_MSG_UNKNOWN));
      break;
    case pms.ERROR_MSG_HEADER:
      Serial.println(F(PMS_ERROR_MSG_HEADER));
      break;
    case pms.ERROR_MSG_BODY:
      Serial.println(F(PMS_ERROR_MSG_BODY));
      break;
    case pms.ERROR_MSG_START:
      Serial.println(F(PMS_ERROR_MSG_START));
      break;
    case pms.ERROR_MSG_LENGTH:
      Serial.println(F(PMS_ERROR_MSG_LENGTH));
      break;
    case pms.ERROR_MSG_CKSUM:
      Serial.println(F(PMS_ERROR_MSG_CKSUM));
      break;
    case pms.ERROR_PMS_TYPE:
      Serial.println(F(PMS_ERROR_PMS_TYPE));
      break;
    }
  }
  pms.sleep();
}

void show_on_display(const std::unique_ptr<Measurement>& measurement) {
  show_on_display(Measurement::HumanReadableFormatter::format(measurement));
  delay(display_each_measurement_for_in_millis);
}

bool send_discovery_messages() {
  for (const auto& sensor_definition : home_assistant_device->sensor_definitions()) {
    ReconnectingPubSubClient::Error publish_error = reconnecting_mqtt_client->publish(home_assistant_device->get_discovery_topic_for(sensor_definition), home_assistant_device->get_discovery_message_for_sensor(sensor_definition));

    if (publish_error != ReconnectingPubSubClient::Error::None) {
      Serial.println("Unable to publish discovery messages");
      return false;
    }
  }

  return true;
}

void loop() {
  server.handleClient();
  if (mqtt_configured) {
    mqtt_client.loop();
  }

  if (is_setup && !web_portal_requested) {
    if (mqtt_configured && !discovery_messages_sent) {
      discovery_messages_sent = send_discovery_messages();
    }

    if (should_read_new_measurements()) {
      measurements.clear();

      read_temperature_and_humidity();
      read_co2();
      read_pms();

      DynamicJsonDocument measurements_json(1024);
      for (const auto& measurement : measurements) {
        measurements_json[device_classes_of_measurement_types[(uint16_t)measurement->get_type()]] = measurement->value_as_string();

        show_on_display(measurement);
      }

      report(measurements_json);

      data_last_read_timestamp = millis();
    }
    else {
      for (const auto& measurement : measurements) {
        show_on_display(measurement);    
      }
    }
  }
}