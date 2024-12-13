#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <WebSocketsClient_Generic.h> // For WebSocket connection
#include "credentials.h"
#include <esp_camera.h>
#include "base64.h"

// Device Info
const String deviceName = "ESP32-CAM-Streamer";
const String deviceType = "Camera-Streamer";
const String deviceId = "1bA";

// Camera Pins (adjust as per your board)
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// WebSocket connection
WebSocketsClient webSocket;

String jwtToken;
String userId;

uint16_t serverPort = 80; // Your server port


// Device data object
StaticJsonDocument<2048> deviceData;  

// Frame interval for 30 fps ~33ms
const int frameInterval = 1000;
unsigned long lastCaptureTime = 0;

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
      StaticJsonDocument<512> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload);

      if (!error) {
        if (jsonDoc.containsKey("messageType")) {
          String receivedType = jsonDoc["messageType"].as<String>();
          if (receivedType == "ping") {
            // Send pong
            String pongMessage = "{\"messageType\": \"pong\", \"message\": \"pong\"}";
            webSocket.sendTXT(pongMessage);
          } else if (receivedType == "userDisconnected") {
            Serial.println("User disconnected");
            deviceData["data"]["active"] = false;
            deviceData["data"].remove("imageBinaryData");
          }
        }

        // Check if we need to update active state or other data
        if (jsonDoc.containsKey("data")) {
          if (jsonDoc["data"].containsKey("active")) {
            if(deviceData["data"]["active"] == false){
              Serial.println("Stop Camera");
            }
            deviceData["data"]["active"] = jsonDoc["data"]["active"];
          }
        }
      } else {
        Serial.println("Failed to parse WebSocket message as JSON.");
      }

      break;
    }

    default:
      break;
  }
}

void sendDeviceInfoToServer() {
  String deviceJson;
  serializeJson(deviceData, deviceJson);
  webSocket.sendTXT(deviceJson);
  Serial.println("Device info sent: " + deviceJson);
}

bool loginToServer() {
  HTTPClient http;
  String loginUrl = "https://"+ serverHost + "/login";
  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  String postData = "{\"email\":\"" + email + "\",\"password\":\"" + acc_pass + "\"}";
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


void captureAndSendFrame() {
  if (!deviceData["data"]["active"]) {
    return; // Skip if the camera is not active
  }

  // Capture a frame from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    return;
  }

  // Check if WebSocket is connected
  if (webSocket.isConnected()) {
    // Send the raw image buffer as binary data
    webSocket.sendBIN(fb->buf, fb->len);
    Serial.printf("Image frame sent: %d bytes\n", fb->len);
  } else {
    Serial.println("WebSocket not connected, skipping frame.");
  }

  // Return the frame buffer to free memory
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi using credentials from credentials.h
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");

  // Initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;  
  config.jpeg_quality = 11;
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    return;
  }

  // Set initial device data
  deviceData["deviceId"] = deviceId;
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["active"] = false;

  if (loginToServer()) {
    // Use serverHost defined in credentials.h to construct the WebSocket path
    String path = "/?token=" + jwtToken +  "&userId=" + userId + "&type=mcu" +"&deviceId=" + deviceId + "&deviceName=" + deviceName + "&deviceType=" + deviceType;
    
    // Connect to WebSocket using the serverHost from credentials.h
    webSocket.begin(serverHost.c_str(), serverPort, path.c_str());
    webSocket.onEvent(webSocketEvent);
  }
}


void loop() {
  webSocket.loop();

  unsigned long currentMillis = millis();
  if (deviceData["data"]["active"] && (currentMillis - lastCaptureTime >= frameInterval)) {
    lastCaptureTime = currentMillis;
    captureAndSendFrame();
  }
}
