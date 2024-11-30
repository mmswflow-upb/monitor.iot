import network
import socket
import ubinascii
import json
import time

# Wi-Fi credentials
SSID = "WiEarth"
PASSWORD = "3TS9fFtZNf"

# WebSocket Server Info
WEBSOCKET_HOST = 'monitor-iot-server-b4531a0ea68c.herokuapp.com'  # Change to your WebSocket server host
WEBSOCKET_PORT = 80  # Default port for WebSocket (for your Heroku server)
LOGIN_URL = 'https://monitor-iot-server-b4531a0ea68c.herokuapp.com/login'  # Change to your login endpoint
device_name = 'RP_Pico_W'
device_type = 'sensor'
client_type = 'mcu'

# User Credentials for Login
email = 'mmswflow@gmail.com'
password = '123456'

# Connect to WiFi
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(SSID, PASSWORD)
    while not wlan.isconnected():
        time.sleep(0.5)
    print("WiFi connected:", wlan.ifconfig())

# Extract User ID from JWT Token
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
    import urequests  # Make sure urequests is available on your RP Pico W
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

# WebSocket connection function
def connect_websocket(user_id, jwt_token):
    websocket_url = f"/socket?token={jwt_token}&userId={user_id}&type={client_type}&deviceName={device_name}&deviceType={device_type}"
    print("Connecting to WebSocket with URL:", websocket_url)

    s = socket.socket()
    addr_info = socket.getaddrinfo(WEBSOCKET_HOST, WEBSOCKET_PORT)
    addr = addr_info[0][-1]
    s.connect(addr)
    
    # WebSocket handshake headers
    headers = [
        f"GET {websocket_url} HTTP/1.1",
        f"Host: {WEBSOCKET_HOST}",
        "Upgrade: websocket",
        "Connection: Upgrade",
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==",
        "Sec-WebSocket-Version: 13",
        "\r\n"
    ]
    s.send("\r\n".join(headers) + "\r\n")
    
    # Read WebSocket response (should be a handshake confirmation)
    response = s.recv(1024)
    if b"101" not in response:
        print("WebSocket handshake failed:", response)
        s.close()
        return None
    
    print("WebSocket connected.")
    return s

# Send message to server and listen for echo
def send_message_and_listen(s, message):
    # Send message
    frame = b"\x81" + bytes([len(message)]) + message.encode('utf-8')
    s.send(frame)
    print(f"Sent message: {message}")

    # Listen for echo response (message from server)
    try:
        while True:
            response = s.recv(1024)
            if response:
                # Extract message from WebSocket frame
                if response[0] == 0x81:
                    message_len = response[1]
                    received_message = response[2:2 + message_len].decode('utf-8')
                    print(f"Received echoed message: {received_message}")
                    return received_message
            time.sleep(0.1)
    except Exception as e:
        print("Error receiving WebSocket message:", e)
        return None

# Main function
def main():
    connect_wifi()
    
    # Log in to the server and get the token
    user_id, jwt_token = login()
    if user_id and jwt_token:
        # Connect to WebSocket server
        s = connect_websocket(user_id, jwt_token)
        if s:
            # Send a test message to the server
            message = "Hello from RP Pico W!"
            send_message_and_listen(s, message)
            s.close()

# Run the main function
main()
