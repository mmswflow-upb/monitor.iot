#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "credentials.h"
#include <WebSocketsClient.h>

// Device Info
const String deviceName = "ESP32-DHT22";
const String deviceType = "Temperature-Humidity-Sensor";
const String deviceId = "1bA";

// DHT22 setup
#define DHTPIN 4 // Pin connected to the DHT22 data pin
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

WebSocketsClient webSocket;

String jwtToken;
String userId;

// Device data object (initialized before connecting to the server)
StaticJsonDocument<512> deviceData;

void setupDeviceData() {
  // Initialize the device data with default values
  deviceData["userId"] = "";  // Initially empty, will be set after login
  deviceData["deviceId"] = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["temperature"] = 0.0;
  deviceData["data"]["humidity"] = 0.0;
}

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected.");
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected.");
      // Send device information as soon as the connection is established
      sendDeviceInfoToServer();
      break;

    case WStype_TEXT: {
      Serial.print("Received message: ");
      Serial.println((char *)payload);
      break;
    }

    default:
      break;
  }
}

bool loginToServer() {
  HTTPClient http;
  String loginUrl = String(serverUrl) + "/login";
  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  String postData = "{\"email\":\"mmswflow@gmail.com\",\"password\":\"123456\"}";
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode <= 0) {
    Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpResponseCode).c_str());
    return false;
  }

  String responseBody = http.getString();
  Serial.println("Server response: " + responseBody);

  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, responseBody);

  if (error || !jsonDoc.containsKey("token")) {
    Serial.println("Invalid server response.");
    return false;
  }

  jwtToken = jsonDoc["token"].as<String>();
  userId = jsonDoc["id"].as<String>();

  // Update device data with userId after successful login
  deviceData["userId"] = userId;

  return true;
}

void sendDeviceInfoToServer() {
  // Convert device object to JSON string
  String deviceJson;
  serializeJson(deviceData, deviceJson);

  // Send the device object via WebSocket
  webSocket.sendTXT(deviceJson);
  Serial.println("Device info sent to the server: " + deviceJson);
}

void connectToWebSocket() {
  String host = "monitor-iot-server-b4531a0ea68c.herokuapp.com"; // WebSocket host
  uint16_t port = 80;                  // Heroku uses port 80 for WebSocket connections

  String path = "/?token=" + jwtToken + 
                "&userId=" + userId + 
                "&type=mcu" + 
                "&deviceId=" + deviceId +
                "&deviceName=" + deviceName + 
                "&deviceType=" + deviceType;

  webSocket.begin(host.c_str(), port, path.c_str(), "");
  webSocket.onEvent(webSocketEvent);
}

// Function to read sensor data
void readSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Update the device data with sensor values
  deviceData["data"]["temperature"] = temperature;
  deviceData["data"]["humidity"] = humidity;

  Serial.println("Sensor data updated:");
  Serial.println("Temperature: " + String(temperature) + "°C");
  Serial.println("Humidity: " + String(humidity) + "%");

  // Send updated data to the server
  sendDeviceInfoToServer();
}

void setup() {
  Serial.begin(115200);

  // Setup device data with default values before any connection
  setupDeviceData();

  // Initialize DHT sensor
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  if (loginToServer()) {
    connectToWebSocket(); 
  }
}

void loop() {
  webSocket.loop();  // Listen for WebSocket messages

  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime >= 5000) { // Read sensor data every 5 seconds
    readSensorData();
    lastSensorReadTime = millis();
  }
}
