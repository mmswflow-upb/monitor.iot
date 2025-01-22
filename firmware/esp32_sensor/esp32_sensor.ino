#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"       // For base64 decoding
#include <WebSocketsClient_Generic.h> // Same WebSockets library as ESP32-CAM
#include "credentials.h"          // Must define ssid, password, serverHost, email, acc_pass
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// -------------------------------------------------------------------------------------------
// Device Info
// -------------------------------------------------------------------------------------------
const String deviceName = "Temperature-Humidity-Sensor";
const String deviceType = "Temperature-Humidity-Sensor";
const String deviceId   = "sensor4";

// -------------------------------------------------------------------------------------------
// DHT22 Setup
// -------------------------------------------------------------------------------------------
#define DHTPIN  4    // Pin connected to the DHT22 data pin
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// -------------------------------------------------------------------------------------------
// WebSocket
// -------------------------------------------------------------------------------------------
WebSocketsClient webSocket;

// -------------------------------------------------------------------------------------------
// Global variables
// -------------------------------------------------------------------------------------------
String jwtToken;
String userId;

// -------------------------------------------------------------------------------------------
// Device data object (same structure, storing sensor data in "data")
// -------------------------------------------------------------------------------------------
StaticJsonDocument<512> deviceData;

void setupDeviceData() {
  deviceData["userId"]     = "";
  deviceData["deviceId"]   = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  // Store temperature & humidity under "data"
  deviceData["data"]["temperature"] = 0.0;
  deviceData["data"]["humidity"]    = 0.0;
}

// -------------------------------------------------------------------------------------------
// Base64 URL decode (identical to the ESP32-CAM approach)
// -------------------------------------------------------------------------------------------
String base64UrlDecode(const String &input) {
  String decodedInput = input;

  // Replace URL-safe characters
  decodedInput.replace('-', '+');
  decodedInput.replace('_', '/');

  // Pad with '=' characters to make the length a multiple of 4
  while (decodedInput.length() % 4 != 0) {
    decodedInput += '=';
  }

  // Decode base64
  size_t outputLen = 0;
  unsigned char outputBuffer[512];

  int result = mbedtls_base64_decode(outputBuffer,
                                     sizeof(outputBuffer),
                                     &outputLen,
                                     (const unsigned char *)decodedInput.c_str(),
                                     decodedInput.length());

  if (result == 0) {
    return String((char *)outputBuffer).substring(0, outputLen);
  } else {
    return "";
  }
}

// -------------------------------------------------------------------------------------------
// Log in to the server (same approach as ESP32-CAM)
// -------------------------------------------------------------------------------------------
bool loginToServer() {
  HTTPClient http;

  // Build the URL in the same way as the ESP32-CAM code
  String loginUrl = "https://" + serverHost + "/login";

  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  // Send credentials in the same format
  String postData = "{\"email\":\"" + String(email) + "\",\"password\":\"" + String(acc_pass) + "\"}";
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode <= 0) {
    Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpResponseCode).c_str());
    return false;
  }

  String responseBody = http.getString();
  Serial.println("Server response: " + responseBody);

  // Parse JSON
  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, responseBody);

  if (error || !jsonDoc.containsKey("token")) {
    Serial.println("Invalid server response.");
    return false;
  }

  jwtToken = jsonDoc["token"].as<String>();

  // Decode the JWT payload (same method as ESP32-CAM)
  String payloadEncoded = jwtToken.substring(jwtToken.indexOf('.') + 1, jwtToken.lastIndexOf('.'));
  String decodedPayload = base64UrlDecode(payloadEncoded);

  if (decodedPayload.isEmpty()) {
    Serial.println("Failed to decode JWT payload.");
    return false;
  }

  // Parse the payload
  StaticJsonDocument<1024> payloadDoc;
  error = deserializeJson(payloadDoc, decodedPayload);

  if (error || !payloadDoc.containsKey("id")) {
    Serial.println("Invalid token payload.");
    return false;
  }

  userId = payloadDoc["id"].as<String>();
  Serial.println("User ID extracted: " + userId);

  // Update device data with userId after successful login
  deviceData["userId"] = userId;

  return true;
}

// -------------------------------------------------------------------------------------------
// WebSocket event handler (mimics the ESP32-CAM code's event handler)
// -------------------------------------------------------------------------------------------
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
      // Just like ESP32-CAM, parse for messageType
      StaticJsonDocument<1024> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload, length);

      if (!error) {
        if (jsonDoc.containsKey("messageType")) {
          String receivedType = jsonDoc["messageType"].as<String>();

          if (receivedType == "ping") {
            // Send pong back to server
            String pongMessage = "{\"messageType\": \"pong\", \"message\": \"pong\"}";
            webSocket.sendTXT(pongMessage);
          } 
          else if (receivedType == "userDisconnected") {
            Serial.println("User disconnected");
          }

          // Stop further processing once known messageType is handled
          break;
        }

        // If there's additional data from the server we want to handle, do it here.
        // (We are not altering temperature/humidity code—just an example placeholder)
        if (jsonDoc.containsKey("data")) {
          // You could parse server-side updates if needed
          Serial.println("Received server data: ");
          serializeJson(jsonDoc["data"], Serial);
          Serial.println();
        }
      }
      break;
    }

    default:
      break;
  }
}

// -------------------------------------------------------------------------------------------
// Send device info to server (same approach as ESP32-CAM)
// -------------------------------------------------------------------------------------------
void sendDeviceInfoToServer() {
  String deviceJson;
  serializeJson(deviceData, deviceJson);

  if (webSocket.isConnected()) {
    webSocket.sendTXT(deviceJson);
    Serial.println("Device info sent to the server: " + deviceJson);
  }
}

// -------------------------------------------------------------------------------------------
// Connect to WebSocket (mirrors the ESP32-CAM approach)
// -------------------------------------------------------------------------------------------
void connectToWebSocket() {
  // Build the path with query parameters (same as ESP32-CAM)
  String path = "/?token=" + jwtToken +
                "&userId=" + userId +
                "&type=mcu" +
                "&deviceId=" + deviceId +
                "&deviceName=" + deviceName +
                "&deviceType=" + deviceType;

  // Use port 80 if your serverHost is an HTTP endpoint.
  // If your server is HTTPS for WS (wss), you might need a secure port (e.g., 443).
  webSocket.begin(serverHost.c_str(), 80, path.c_str(), "");
  webSocket.onEvent(webSocketEvent);
}

// -------------------------------------------------------------------------------------------
// Function to read sensor data (unchanged)
// -------------------------------------------------------------------------------------------
void readSensorData() {
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Update the device data with sensor values
  deviceData["data"]["temperature"] = temperature;
  deviceData["data"]["humidity"]    = humidity;

  Serial.println("Sensor data updated:");
  Serial.println("Temperature: " + String(temperature) + "°C");
  Serial.println("Humidity: " + String(humidity) + "%");

  // Send updated data to the server
  sendDeviceInfoToServer();
}

// -------------------------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Setup device data
  setupDeviceData();

  // Initialize the DHT sensor
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Log in to the server; if success, connect to WebSocket
  if (loginToServer()) {
    connectToWebSocket();
  }
}

// -------------------------------------------------------------------------------------------
// Loop
// -------------------------------------------------------------------------------------------
void loop() {
  // Keep the WebSocket connection alive
  webSocket.loop();

  // Read sensor data every 5 seconds (example interval)
  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime >= 5000) {
    readSensorData();
    lastSensorReadTime = millis();
  }
}
