#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// WiFi credentials
const char* ssid = "WiEarth";
const char* password = "3TS9fFtZNf";

// Server URLs and credentials
const char* login_url = "http://monitor-iot-server-b4531a0ea68c.herokuapp.com/login";
const char* websocket_host = "ws://monitor-iot-server-b4531a0ea68c.herokuapp.com:8080";

// Login credentials
const char* email = "mmswflow@gmail.com";
const char* password_http = "123456";

// Device parameters for WebSocket
const char* device_name = "RP_Pico_W";
const char* device_type = "Sensor";
const char* client_type = "mcu";

WebsocketsClient client;

// Connect to WiFi
void connectWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

// Helper function for Base64 decoding
String base64Decode(String input) {
    int len = input.length();
    String decodedString;
    int val = 0, valb = -8;
    for (int jj = 0; jj < len; jj++) {
        unsigned char c = input[jj];
        if (c >= 'A' && c <= 'Z') c = c - 'A';
        else if (c >= 'a' && c <= 'z') c = c - 'a' + 26;
        else if (c >= '0' && c <= '9') c = c - '0' + 52;
        else if (c == '+') c = 62;
        else if (c == '/') c = 63;
        else continue;
        val = (val << 6) + c;
        valb += 6;
        if (valb >= 0) {
            decodedString += (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return decodedString;
}

// Decode JWT to extract user ID
String extractUserId(const String& jwt) {
    int firstDot = jwt.indexOf('.');
    int secondDot = jwt.indexOf('.', firstDot + 1);
    if (firstDot == -1 || secondDot == -1) return "";
    String payload = jwt.substring(firstDot + 1, secondDot);
    
    // Decode base64 payload
    String decodedPayload = base64Decode(payload);
    
    // Parse JSON payload
    StaticJsonDocument<256> doc;
    deserializeJson(doc, decodedPayload);
    return doc["id"].as<String>();
}

// Login to the server and retrieve token
String login(String& jwtToken) {
    HTTPClient http;
    http.begin(login_url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> loginDoc;
    loginDoc["email"] = email;
    loginDoc["password"] = password_http;

    String requestBody;
    serializeJson(loginDoc, requestBody);
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == 200) {
        StaticJsonDocument<256> responseDoc;
        deserializeJson(responseDoc, http.getString());
        jwtToken = responseDoc["token"].as<String>();
        http.end();
        return extractUserId(jwtToken);
    } else {
        Serial.println("Login failed: " + String(httpResponseCode));
        http.end();
        return "";
    }
}

// Connect to WebSocket server
void connectWebSocket(const String& userId, const String& jwt) {
    String websocketUrl = websocket_host;
    websocketUrl += "/?token=" + jwt + "&userId=" + userId + "&type=" + client_type +
                    "&deviceName=" + device_name + "&deviceType=" + device_type;

    Serial.print("Connecting to WebSocket at: ");
    Serial.println(websocketUrl);

    client.onMessage([](WebsocketsMessage message) {
        Serial.print("Received: ");
        Serial.println(message.data());
    });

    client.onEvent([](WebsocketsEvent event, String data) {
        if (event == WebsocketsEvent::ConnectionOpened) {
            Serial.println("WebSocket connection opened");
        } else if (event == WebsocketsEvent::ConnectionClosed) {
            Serial.println("WebSocket connection closed");
        }
    });

    if (client.connect(websocketUrl.c_str())) {
        StaticJsonDocument<256> doc;
        doc["deviceName"] = device_name;
        doc["deviceType"] = device_type;
        doc["clientType"] = client_type;
        doc["userID"] = userId;

        String message;
        serializeJson(doc, message);
        client.send(message);
        Serial.println("Data sent through WebSocket: " + message);
    } else {
        Serial.println("WebSocket connection failed.");
    }
}

void setup() {
    Serial.begin(115200);
    connectWiFi();

    String jwtToken;
    String userId = login(jwtToken);
    if (userId.length() > 0 && jwtToken.length() > 0) {
        connectWebSocket(userId, jwtToken);
    } else {
        Serial.println("Authentication failed, cannot proceed.");
    }
}

void loop() {
    if (client.available()) {
        client.poll();
    }
    delay(100);
}
