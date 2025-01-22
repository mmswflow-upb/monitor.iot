#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"       // For Base64 decoding
#include <WebSocketsClient_Generic.h>
#include "credentials.h"

// Device Info
const String deviceName = "ESP32-RGB-Lamp";
const String deviceType = "RGB-Lamp";
const String deviceId   = "lamp123"; // example ID

// RGB LED Pins (common anode)
#define RED_PIN   18
#define GREEN_PIN 19
#define BLUE_PIN  21

WebSocketsClient webSocket;

String jwtToken;
String userId;

// Default RGB values (initial values can be set to 0)
int r = 0, g = 200, b = 0;

// Device data object (initialized before connecting)
StaticJsonDocument<512> deviceData;

void setupDeviceData() {
  deviceData["userId"]     = "";
  deviceData["deviceId"]   = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["r"]  = r;
  deviceData["data"]["g"]  = g;
  deviceData["data"]["b"]  = b;
}

// ---------------------------------------------------------------------------
// Base64 URL decoding function (same logic as in your sensor code)
// ---------------------------------------------------------------------------
String base64UrlDecode(const String &input) {
  String decodedInput = input;
  decodedInput.replace('-', '+');
  decodedInput.replace('_', '/');

  while (decodedInput.length() % 4 != 0) {
    decodedInput += '=';
  }

  size_t outputLen = 0;
  unsigned char outputBuffer[512];

  int result = mbedtls_base64_decode(
    outputBuffer, sizeof(outputBuffer), &outputLen,
    (const unsigned char *)decodedInput.c_str(),
    decodedInput.length()
  );

  if (result == 0) {
    return String((char *)outputBuffer).substring(0, outputLen);
  } else {
    return "";
  }
}

// ---------------------------------------------------------------------------
// Log in to the server using HTTPS + JSON credentials
// ---------------------------------------------------------------------------
bool loginToServer() {
  HTTPClient http;
  
  // Same structure as your temp/humidity sensor code
  String loginUrl = "https://" + serverHost + "/login";

  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  // Using `email` and `acc_pass` from credentials.h
  String postData = "{\"email\":\"" + String(email) + "\",\"password\":\"" + String(acc_pass) + "\"}";
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

  // Extract the token
  jwtToken = jsonDoc["token"].as<String>();

  // Decode the JWT payload
  String payloadEncoded = jwtToken.substring(jwtToken.indexOf('.') + 1, jwtToken.lastIndexOf('.'));
  String decodedPayload = base64UrlDecode(payloadEncoded);

  if (decodedPayload.isEmpty()) {
    Serial.println("Failed to decode JWT payload.");
    return false;
  }

  // Parse the payload to get userId
  StaticJsonDocument<1024> payloadDoc;
  error = deserializeJson(payloadDoc, decodedPayload);

  if (error || !payloadDoc.containsKey("id")) {
    Serial.println("Invalid token payload.");
    return false;
  }

  userId = payloadDoc["id"].as<String>();
  Serial.println("User ID extracted: " + userId);

  // Update device data with userId
  deviceData["userId"] = userId;

  return true;
}

// ---------------------------------------------------------------------------
// Send device info over WebSocket
// ---------------------------------------------------------------------------
void sendDeviceInfoToServer() {
  String deviceJson;
  serializeJson(deviceData, deviceJson);

  if (webSocket.isConnected()) {
    webSocket.sendTXT(deviceJson);
    Serial.println("Device info sent to the server: " + deviceJson);
  }
}

// ---------------------------------------------------------------------------
// WebSocket event handler (similar to sensor code approach)
// ---------------------------------------------------------------------------
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected.");
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected.");
      // Send initial device info
      sendDeviceInfoToServer();
      break;

    case WStype_TEXT: {
      // Parse the JSON
      StaticJsonDocument<1024> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload, length);

      if (!error) {
        // Check for messageType (like "ping" or "userDisconnected")
        if (jsonDoc.containsKey("messageType")) {
          String receivedType = jsonDoc["messageType"].as<String>();

          if (receivedType == "ping") {
            webSocket.sendTXT("{\"messageType\":\"pong\",\"message\":\"pong\"}");
          } else if (receivedType == "userDisconnected") {
            Serial.println("User disconnected");
          }
          break;
        }

        // If it contains "data" with new RGB values, update
        if (jsonDoc.containsKey("data")) {
          if (jsonDoc["data"].containsKey("r")) r = jsonDoc["data"]["r"].as<int>();
          if (jsonDoc["data"].containsKey("g")) g = jsonDoc["data"]["g"].as<int>();
          if (jsonDoc["data"].containsKey("b")) b = jsonDoc["data"]["b"].as<int>();

          Serial.printf("New RGB: %d, %d, %d\n", r, g, b);

          // Common Anode: invert the PWM
          analogWrite(RED_PIN,   255 - r);
          analogWrite(GREEN_PIN, 255 - g);
          analogWrite(BLUE_PIN,  255 - b);

          // Update deviceData
          deviceData["data"]["r"] = r;
          deviceData["data"]["g"] = g;
          deviceData["data"]["b"] = b;

          sendDeviceInfoToServer();
        }
      } else {
        Serial.print("Failed to parse WebSocket message: ");
        Serial.println(error.c_str());
      }
      break;
    }

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Connect to WebSocket (same style as sensor code)
// ---------------------------------------------------------------------------
void connectToWebSocket() {
  // Build the path
  String path = "/?token=" + jwtToken +
                "&userId=" + userId +
                "&type=mcu" +
                "&deviceId=" + deviceId +
                "&deviceName=" + deviceName +
                "&deviceType=" + deviceType;

  // If the server is ws:// on port 80:
  webSocket.begin(serverHost.c_str(), 80, path.c_str(), "");
  webSocket.onEvent(webSocketEvent);
}

void setup() {
  Serial.begin(115200);

  // Initialize device data
  setupDeviceData();

  // Initialize pins
  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);

  // Connect Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Perform login, then connect WebSocket
  if (loginToServer()) {
    connectToWebSocket();
  }
}

void loop() {
  webSocket.loop(); 
}
