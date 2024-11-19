#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h" // For Base64 decoding
#include <WebSocketsClient_Generic.h> // For WebSocket connection

/*
 WiEarth
3TS9fFtZNf
mmswflow@gmail.com
123456
http://monitor-iot-server-b4531a0ea68c.herokuapp.com
ws://monitor-iot-server-b4531a0ea68c.herokuapp.com
 */

const char* ssid = "WiEarth";           
const char* password = "3TS9fFtZNf";   
const String serverUrl = "https://monitor-iot-server-b4531a0ea68c.herokuapp.com";
const String socketUrl = "wss://monitor-iot-server-b4531a0ea68c.herokuapp.com";

const String deviceName = "esp32-Cam";
const String deviceType = "sensor";

String jwtToken;
String userId;

WebSocketsClient webSocket;

#include <ArduinoJson.h>

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected.");
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected.");
      webSocket.sendTXT("Hello from ESP32!");
      break;

    case WStype_TEXT: {
      Serial.print("WebSocket message received: ");
      Serial.println((char *)payload);

      StaticJsonDocument<512> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload);

      if (!error) {
        Serial.println("Parsed JSON Data:");
        for (JsonPair keyValue : jsonDoc.as<JsonObject>()) {
          String key = keyValue.key().c_str();
          String value;
          
          // Determine the type of the value and handle it accordingly
          if (keyValue.value().is<const char*>()) {
            value = String(keyValue.value().as<const char*>());
          } else if (keyValue.value().is<int>()) {
            value = String(keyValue.value().as<int>());
          } else if (keyValue.value().is<float>()) {
            value = String(keyValue.value().as<float>());
          } else if (keyValue.value().is<bool>()) {
            value = keyValue.value().as<bool>() ? "true" : "false";
          } else {
            value = "Unknown type";
          }

          Serial.println("Key: " + key + ", Value: " + value);
        }
      } else {
        Serial.println("Failed to parse WebSocket message as JSON.");
        Serial.print("Error: ");
        Serial.println(error.c_str());
      }
      break;
    }

    default:
      break;
  }
}



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

  int result = mbedtls_base64_decode(outputBuffer, sizeof(outputBuffer), &outputLen,
                                     (const unsigned char *)decodedInput.c_str(), decodedInput.length());

  if (result == 0) {
    return String((char *)outputBuffer).substring(0, outputLen);
  } else {
    Serial.println("Base64 decoding failed.");
    return "";
  }
}

bool loginToServer() {
  HTTPClient http;
  String loginUrl = serverUrl + "/login";
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

  // Decode the JWT payload
  String payloadEncoded = jwtToken.substring(jwtToken.indexOf('.') + 1, jwtToken.lastIndexOf('.'));
  String decodedPayload = base64UrlDecode(payloadEncoded);

  if (decodedPayload.isEmpty()) {
    Serial.println("Failed to decode JWT payload.");
    return false;
  }

  Serial.println("Decoded JWT Payload: " + decodedPayload);

  StaticJsonDocument<1024> payloadDoc;
  error = deserializeJson(payloadDoc, decodedPayload);

  if (error || !payloadDoc.containsKey("id")) {
    Serial.println("Invalid token payload.");
    return false;
  }

  userId = payloadDoc["id"].as<String>();
  Serial.println("User ID extracted: " + userId);
  return true;
}
void connectToWebSocket() {
  String host = "monitor-iot-server-b4531a0ea68c.herokuapp.com"; // WebSocket host
  uint16_t port = 80;                  // Heroku uses port 80 for WebSocket connections

  String path = "/?token=" + jwtToken + 
                "&userId=" + userId + 
                "&type=mcu" + 
                "&deviceName=" + deviceName + 
                "&deviceType=" + deviceType;

  webSocket.begin(host.c_str(), port, path.c_str(), "");
  webSocket.onEvent(webSocketEvent);
}


void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  if (loginToServer()) connectToWebSocket();
}

void loop() {
  webSocket.loop();
}
