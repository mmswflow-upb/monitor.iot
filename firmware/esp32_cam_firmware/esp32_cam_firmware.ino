#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <WebSocketsClient_Generic.h> // For WebSocket connection
#include "credentials.h"
#include <esp_camera.h>
#include <base64.h>
// Device Info
const String deviceName = "ESP32-CAM-Streamer";
const String deviceType = "Camera";
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
const int frameInterval = 200;

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
      StaticJsonDocument<1024> jsonDoc;
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
                deviceData["data"]["active"] = false;
                deviceData["data"]["binaryFrame"] = "";
            }

            // Exit the current handling logic
            break;
        }


        // You can also update other fields from the server (e.g., RGB values, other device settings)
        if (jsonDoc.containsKey("data")) {
          if (jsonDoc["data"].containsKey("active")) {
         
            if(jsonDoc["data"]["active"] == false && deviceData["data"]["active"] == true){

              deviceData["data"]["binaryFrame"] = "";
              deviceData["data"]["active"] = jsonDoc["data"]["active"];

              sendDeviceInfoToServer();
            }else{
              deviceData["data"]["active"] = jsonDoc["data"]["active"];
            }
            
           
          }
        }
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
  if(webSocket.isConnected()){
    webSocket.sendTXT(deviceJson);
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
    return "";
  }
}

bool loginToServer() {
  // Same login process as your original code
  HTTPClient http;
  String loginUrl = "https://" + serverHost + "/login";
  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");

  String postData = "{\"email\":\"" + email + "\",\"password\":\"" + acc_pass + "\"}";

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode <= 0) {
    return false;
  }

  String responseBody = http.getString();

  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, responseBody);

  if (error || !jsonDoc.containsKey("token")) {
    return false;
  }

  jwtToken = jsonDoc["token"].as<String>();
   // Decode the JWT payload
  String payloadEncoded = jwtToken.substring(jwtToken.indexOf('.') + 1, jwtToken.lastIndexOf('.'));
  String decodedPayload = base64UrlDecode(payloadEncoded);

  if (decodedPayload.isEmpty()) {
    return false;
  }


  StaticJsonDocument<1024> payloadDoc;
  error = deserializeJson(payloadDoc, decodedPayload);

  if (error || !payloadDoc.containsKey("id")) {
    return false;
  }

  userId = payloadDoc["id"].as<String>();
  
  // Update device data with userId after successful login
  deviceData["userId"] = userId;

  return true;
}

void captureFrame() {
  camera_fb_t *fb = esp_camera_fb_get();  // Capture a frame from the camera

  if (fb) {
    // Convert the image to base64 (optional step for encoding)
    deviceData["data"]["binaryFrame"] = base64::encode(fb->buf, fb->len);
    
    //Send info to server
    sendDeviceInfoToServer();

    // Free the frame buffer
    deviceData["data"]["binaryFrame"] = "";
    esp_camera_fb_return(fb);

  }
}

void setupDeviceData(){
  // Set initial values for deviceData (this can be updated dynamically later)
  deviceData["deviceId"] = deviceId;
  deviceData["userId"] = "";
  deviceData["deviceName"] = deviceName;
  deviceData["deviceType"] = deviceType;
  deviceData["data"]["active"] = false;

}

void connectToWebSocket(){
    String path = "/?token=" + jwtToken  + "&userId=" + userId + "&type=mcu" + "&deviceId=" + deviceId + "&deviceName=" + deviceName + "&deviceType=" + deviceType;
    webSocket.begin(serverHost.c_str(), 80, path.c_str(), "");
    webSocket.onEvent(webSocketEvent);
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
  config.xclk_freq_hz = 20000000;       // 20 MHz
  config.pixel_format = PIXFORMAT_JPEG; // Use JPEG format
  config.frame_size = FRAMESIZE_SVGA;  // Smallest frame size for testing
  config.jpeg_quality = 12;             // High compression
  config.fb_count = 1;                  // Single buffer       // Increase compression

  if(esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
  }else{
    Serial.println("Camera initialized Successfully!");
  }

  setupDeviceData();
  // If login is successful, connect to WebSocket
  if (loginToServer()) {
    
    connectToWebSocket();
  }
}

void loop() {
  webSocket.loop();  // Maintain WebSocket connection
  unsigned long currentMillis = millis();

  // Capture and send frames at 30fps (every 33ms) only if 'active' is true
  static unsigned long lastCaptureTime = 0;
  if (deviceData["data"]["active"] && (currentMillis - lastCaptureTime >= frameInterval)) {
    lastCaptureTime = currentMillis;
    captureFrame();
  }
}