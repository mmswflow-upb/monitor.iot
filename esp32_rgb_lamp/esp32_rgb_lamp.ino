#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h" // For Base64 decoding
#include <WebSocketsClient_Generic.h> // For WebSocket connection
#include "credentials.h"



// Device Info
const String deviceName = "ESP32-RGB-Lamp";
const String deviceType = "RGB-Lamp";
const String deviceId = "1bA";

// RGB LED Pins (common anode)
#define RED_PIN 15
#define GREEN_PIN 2
#define BLUE_PIN 14

WebSocketsClient webSocket;

String jwtToken;
String userId;

// Default RGB values (initial values can be set to 0)
int r = 0, g = 200, b = 0;

// Device data object (initialized before connecting to the server)
StaticJsonDocument<512> deviceData;

void setupDeviceData() {
  // Initialize the device data with default values
  deviceData["userId"] = "";  // Initially empty, will be set after login
  deviceData["deviceId"] = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["r"] = r;
  deviceData["data"]["g"] = g;
  deviceData["data"]["b"] = b;
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
      

      StaticJsonDocument<512> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload);

      if (!error) {
        if (jsonDoc.containsKey("messageType")) {
            String receivedType = jsonDoc["messageType"].as<String>();

            if (receivedType == "ping") {
                // Handle the "ping" type
                String pongMessage = "{\"messageType\": \"pong\", \"message\": \"pong\"}";
                webSocket.sendTXT(pongMessage); // Send pong back to server
            } 
            else if (receivedType == "userDisconnected") {
                Serial.println("User disconnected");
            }

            // Exit the current handling logic
            break;
        }

        // Check if the message contains RGB values
        if(jsonDoc.containsKey("data")){
          Serial.print("Device Object Updated: ");
          Serial.println((char *)payload);
          
          
            // Update the global RGB values
            r = jsonDoc["data"]["r"].as<int>();
            g = jsonDoc["data"]["g"].as<int>();
            b = jsonDoc["data"]["b"].as<int>();

            Serial.println("New RGB: " + String(r) + ", " + String(g) + ", " + String(b));

            // Since it's a common anode RGB LED, invert the PWM values
            analogWrite(RED_PIN, 255 - r);   // Invert red PWM value
            analogWrite(GREEN_PIN, 255 - g); // Invert green PWM value
            analogWrite(BLUE_PIN, 255 - b);  // Invert blue PWM value

            // Update deviceData with new color values
            deviceData["data"]["r"] = r;
            deviceData["data"]["g"] = g;
            deviceData["data"]["b"] = b;
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

void setup() {
  Serial.begin(115200);

  // Setup device data with default values before any connection
  setupDeviceData();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println("Connected to WiFi");

  // Initialize RGB LED pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

 

  if (loginToServer()) {
    connectToWebSocket(); 
  }
}

void loop() {
  webSocket.loop();  // Listen for WebSocket messages
}
