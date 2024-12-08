#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <WebSocketsClient_Generic.h> // For WebSocket connection
#include "credentials.h"
#include <esp_camera.h>

// Device Info
const String deviceName = "ESP32-CAM-Streamer";
const String deviceType = "Camera-Streamer";
const String deviceId = "1bA";

// Camera Pins (change based on your ESP32-CAM model)
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// WebSocket connection
WebSocketsClient webSocket;

String jwtToken;
String userId;


// Device data object (initializing with default values)
StaticJsonDocument<512> deviceData;  // You can expand this size as needed

// Capture Frame Interval (to achieve 30fps, 33ms per frame)
const int frameInterval = 33;

// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected.");
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected.");
      sendDeviceInfoToServer();  // Send initial device info
      break;

    case WStype_TEXT: {
      // Deserialize the incoming message to check for the active setting
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


        // You can also update other fields from the server (e.g., RGB values, other device settings)
        if (jsonDoc.containsKey("data")) {
          if (jsonDoc["data"].containsKey("active")) {
         
            deviceData["data"]["active"] = jsonDoc["data"]["active"];
           
          }
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

void sendDeviceInfoToServer() {
  // Convert deviceData to a JSON string and send it over WebSocket
  String deviceJson;
  serializeJson(deviceData, deviceJson);
  webSocket.sendTXT(deviceJson);
  Serial.println("Device info sent to the server: " + deviceJson);
}

bool loginToServer() {
  // Same login process as your original code
  HTTPClient http;
  String loginUrl = serverUrl + "/login";
  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  String postData = "{\"email\":\"your-email@example.com\",\"password\":\"your-password\"}";
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
  return true;
}

void captureAndSendFrame() {
  camera_fb_t *fb = esp_camera_fb_get();  // Capture a frame from the camera

  if (fb) {
    // Convert the image to base64 (optional step for encoding)
    String base64Image = base64::encode(fb->buf, fb->len);

    // Prepare WebSocket message (you can modify this to add other metadata if needed)
    String imageMessage = "{\"type\": \"image\", \"data\": \"" + base64Image + "\"}";

    // Send the image over WebSocket if the connection is active
    if (webSocket.isConnected()) {
      webSocket.sendTXT(imageMessage);
    }

    // Free the frame buffer
    esp_camera_fb_return(fb);

    Serial.println("Image sent: " + String(fb->len) + " bytes");
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 32;
  config.pin_d1 = 35;
  config.pin_d2 = 34;
  config.pin_d3 = 23;
  config.pin_d4 = 22;
  config.pin_d5 = 21;
  config.pin_d6 = 19;
  config.pin_d7 = 18;
  config.pin_xclk = 0;
  config.pin_pclk = 4;
  config.pin_vsync = 5;
  config.pin_href = 13;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA; // You can adjust resolution here
  config.pixel_format = PIXFORMAT_JPEG;

  esp_camera_init(&config);

  // Set initial values for deviceData (this can be updated dynamically later)
  deviceData["deviceId"] = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["active"] = false;

  // If login is successful, connect to WebSocket
  if (loginToServer()) {
    String host = "your-websocket-server.com";
    String path = "/stream?token=" + jwtToken + "&userId=" + userId + "&deviceId=" + deviceId;
    webSocket.begin(host.c_str(), 80, path.c_str(), "");
    webSocket.onEvent(webSocketEvent);
  }
}

void loop() {
  webSocket.loop();  // Maintain WebSocket connection
  unsigned long currentMillis = millis();

  // Capture and send frames at 30fps (every 33ms) only if 'active' is true
  static unsigned long lastCaptureTime = 0;
  if (deviceData["data"]["active"] && (currentMillis - lastCaptureTime >= frameInterval)) {
    lastCaptureTime = currentMillis;
    captureAndSendFrame();
  }
}
