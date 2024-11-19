import network
import urequests
import usocket as socket
import time
import json
import ubinascii

# WiFi credentials
SSID = 'IoTProjectWifi'
PASSWORD = '12345678'

# Server credentials and details
# Server URLs
LOGIN_URL = 'http://127.0.0.1:8080/login'
WEBSOCKET_HOST = 'ws://127.0.0.1:8080'
WEBSOCKET_PORT=8080
# Login details
email = 'mmswflow@gmail.com'
password = '123456'

# Device parameters for WebSocket
device_name = 'RP_Pico_W'
device_type = 'Sensor'
client_type = 'mcu'


# Connect to WiFi
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(SSID, PASSWORD)
    while not wlan.isconnected():
        print("Connecting to WiFi...")
        time.sleep(1)
    print("Connected to WiFi:", wlan.ifconfig())

# Decode JWT token to extract user ID
def extract_user_id(jwt):
    try:
        parts = jwt.split('.')
        if len(parts) == 3:
            payload_b64 = parts[1]
            payload_b64 += '=' * (-len(payload_b64) % 4)
            decoded_payload = ubinascii.a2b_base64(payload_b64).decode('utf-8')
            payload = json.loads(decoded_payload)
            user_id = payload.get('id')
            print("Extracted user ID:", user_id)
            return user_id
        else:
            print("Invalid JWT format.")
            return None
    except Exception as e:
        print("Error decoding JWT:", e)
        return None

# Login to the server and retrieve token
def login():
    headers = {'Content-Type': 'application/json'}
    login_data = json.dumps({'email': email, 'password': password})
    
    print("Logging in...")
    response = urequests.post(LOGIN_URL, headers=headers, data=login_data)
    if response.status_code == 200:
        print("Login successful.")
        token_data = response.json()
        jwt_token = token_data.get('token')
        user_id = extract_user_id(jwt_token)
        response.close()
        return user_id, jwt_token
    else:
        print("Login failed:", response.status_code, response.text)
        response.close()
        return None, None

# Establish WebSocket connection with query parameters, using the /socket path
def connect_websocket(user_id, jwt):
    # Construct WebSocket URL with the /socket path and query parameters
    websocket_url = f"/socket?token={jwt}&userId={user_id}&type={client_type}&deviceName={device_name}&deviceType={device_type}"
    
    print("Connecting to WebSocket with URL:", websocket_url)
    
    s = None
    try:
        addr_info = socket.getaddrinfo(WEBSOCKET_HOST, WEBSOCKET_PORT)
        addr = addr_info[0][-1]
        
        # Create and connect socket
        s = socket.socket()
        
        s.connect(addr)
        
        # WebSocket handshake headers, updated with the Host header for Nginx
        headers = [
            f"GET {websocket_url} HTTP/1.1",
            f"Host: {WEBSOCKET_HOST}",
            "Upgrade: websocket",
            "Connection: Upgrade",
            "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==",
            "Sec-WebSocket-Version: 13",
            "\r\n"  # End of headers
        ]
        s.send("\r\n".join(headers) + "\r\n")

        # Receive and check handshake response
        response = s.recv(1024)
        if b"101" not in response:
            print("WebSocket handshake failed:", response)
            s.close()
            return
        print("WebSocket connected successfully.")

        # Prepare data for WebSocket transmission
        socket_data = json.dumps({
            'deviceName': device_name,
            'deviceType': device_type,
            'clientType': client_type,
            'userID': user_id
        })
        
        # Frame the message as a WebSocket text frame
        s.send(b"\x81" + bytes([len(socket_data)]) + socket_data.encode('utf-8'))
        print("Data sent through WebSocket.")
        
        # Receive server response
        response = s.recv(1024)
        print("Server response:", response.decode('utf-8'))
        
    except OSError as e:
        print("Socket error:", e)
    finally:
        # Close the WebSocket connection if socket was created
        if s:
            s.close()

# Main execution
def main():
    connect_wifi()
    user_id, jwt = login()
    if user_id and jwt:
        connect_websocket(user_id, jwt)
    else:
        print("Authentication failed, cannot proceed.")

main()