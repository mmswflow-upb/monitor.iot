import 'dart:async';

import 'package:flutter/material.dart';
import 'package:web_socket_channel/io.dart';
import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'login_page.dart';
import '../home widgets/device_grid.dart';
import '../home widgets/device_popup_dialog.dart';
import 'package:monitor_iot/theme_manager.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  late IOWebSocketChannel channel;
  final ValueNotifier<List<Map<String, dynamic>>> devicesNotifier =  ValueNotifier<List<Map<String, dynamic>>>([]);
  final Map<String, Completer<void>> openPopups = {}; // Track open popups by deviceId

  final Color _currentColor = Colors.white; // Default color for the RGB lamp

  @override
  void initState() {
    super.initState();
    _connectWebSocket();
  }

  void _connectWebSocket() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    String? token = prefs.getString('jwt_token');
    String? userId = prefs.getString('user_id');

    if (token != null && userId != null) {
      final String? apiUrl = dotenv.env['REMOTE_API_SOCKET_URL'];

      if (apiUrl == null) {
        _showDialog("Error", "WebSocket URL is not set. Please check the configuration.");
        return;
      }

      String clientType = "user";
      final uri = Uri.parse('$apiUrl/?token=$token&userId=$userId&type=$clientType');

      channel = IOWebSocketChannel.connect(uri);

      channel.stream.listen((message) {
        try {
          final decoded = jsonDecode(message);
          if (decoded['messageType'] == 'updateDevicesList') {
            _updateDevicesList(List<Map<String, dynamic>>.from(decoded['devices']));
          } else if (decoded['messageType'] == 'ping') {
            final pongResponse = jsonEncode({'messageType': 'pong', 'message': 'pong'});
            channel.sink.add(pongResponse);
          }
        } catch (e) {
        }
      }, onError: (error) {
        removeToken();
        _showDialog("Error", "Automatic login failed. You need to log in manually.");
      }, onDone: () {
        removeToken();
        _showDialog("Connection Ended", "Connection was closed from the remote server.");
        _navigateToLogin();
      });
    } else {
      _navigateToLogin();
    }
  }

  void _updateDevicesList(List<Map<String, dynamic>> updatedDevices) {
    final currentDeviceList = devicesNotifier.value;
    final updatedDeviceIds = updatedDevices.map((d) => d['deviceId']).toSet();
    final currentDeviceIds = currentDeviceList.map((d) => d['deviceId']).toSet();

    // Close popups for removed devices
    for (final removedDeviceId in currentDeviceIds.difference(updatedDeviceIds)) {
      if (openPopups.containsKey(removedDeviceId)) {
        openPopups[removedDeviceId]?.complete(); // Signal the popup to close
        openPopups.remove(removedDeviceId);
      }
    }

    // Update devices list
    final updatedDeviceList = [...updatedDevices];
    devicesNotifier.value = updatedDeviceList;
  }

  Future<void> removeToken() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    await prefs.remove('jwt_token');
    await prefs.remove('user_id');
  }

  void _navigateToLogin() {
    Navigator.pushReplacement(
      context,
      MaterialPageRoute(builder: (context) => const LoginPage()),
    );
  }

  void _showDialog(String title, String content) {
    if(!mounted) return;

    showDialog(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: Text(title),
          content: Text(content),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('OK'),
            ),
          ],
        );
      },
    );
  }

  void _showDevicePopup(Map<String, dynamic> device) {
    final deviceId = device['deviceId'];

    // Find the latest device data
    final refreshedDevice = devicesNotifier.value.firstWhere(
          (d) => d['deviceId'] == deviceId,
      orElse: () => device,
    );

    // Create a ValueNotifier with the latest device data
    final deviceNotifier = ValueNotifier<Map<String, dynamic>>(refreshedDevice);

    // Close existing popup for the device if it's open
    if (openPopups.containsKey(deviceId)) {
      openPopups[deviceId]?.complete();
      openPopups.remove(deviceId);
    }

    // Track the new popup
    final popupCompleter = Completer<void>();
    openPopups[deviceId] = popupCompleter;

    // Update the popup dynamically when new data is received
    devicesNotifier.addListener(() {
      final updatedDevice = devicesNotifier.value.firstWhere(
            (d) => d['deviceId'] == deviceId,
        orElse: () => deviceNotifier.value,
      );

      // If device is removed from the list, also complete the popup (close it)
      if (!devicesNotifier.value.any((d) => d['deviceId'] == deviceId)) {
        if (!popupCompleter.isCompleted) {
          popupCompleter.complete();
        }
        return;
      }

      if (updatedDevice != deviceNotifier.value) {
        deviceNotifier.value = updatedDevice;
      }
    });

    showDialog(
      context: context,
      builder: (BuildContext context) {
        return DevicePopupDialog(
          deviceNotifier: deviceNotifier,
          currentColor: _currentColor,
          onColorChanged: (Color color) {
            // Update the RGB values in the device's data
            final updatedDevice = {
              ...deviceNotifier.value,
              'data': {
                ...deviceNotifier.value['data'],
                'r': color.red, // Update red value
                'g': color.green, // Update green value
                'b': color.blue, // Update blue value
              },
            };

            deviceNotifier.value = updatedDevice;

            // Optionally sync the state to the server or log for debugging
          },


          onSetColorPressed: () {
            // Optionally, do something with `deviceNotifier.value`:
            // e.g., send an update to the server
            final device = deviceNotifier.value;

            _updateDeviceState(device); // If you have a function to sync to server
          },

          onActiveChanged: (bool active) {
            final updatedDevice = {
              ...deviceNotifier.value,
              'data': {
                ...deviceNotifier.value['data'],
                'active': active,
                'binaryFrame': active ? deviceNotifier.value['data']['binaryFrame'] : '',
              },
            };

            // Clear binaryFrame if camera is deactivated
            if (!active) {
              updatedDevice['data']['binaryFrame'] = '';
            }

            deviceNotifier.value = updatedDevice;
            _updateDeviceState(updatedDevice);
          },
        );
      },
    ).then((_) {
      // Cleanup after dialog is closed manually
      openPopups.remove(deviceId);
    });

    // Automatically close popup if the device is removed
    popupCompleter.future.then((_) {
      if (Navigator.canPop(context)) {
        Navigator.of(context).pop();
      }
    });
  }






  void _updateDeviceState(Map<String, dynamic> device) {
    try {

      if(device['deviceType'] == 'Camera'){
        device['data']['binaryFrame'] = '';
      }
      final message = jsonEncode({
        "deviceId": device["deviceId"],
        "deviceName": device["deviceName"],
        "deviceType": device["deviceType"],
        "data": device["data"], // Contains "active" or other data
      });



      channel.sink.add(message);

    } catch (e) {
      _showDialog("Error", "Failed to update the device state.");
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Home - Monitor.IoT'),
        automaticallyImplyLeading: false,
        actions: [
          IconButton(
            icon: Icon(
              ThemeManager.themeNotifier.value == ThemeMode.light
                  ? Icons.dark_mode
                  : Icons.light_mode,
            ),
            onPressed: () {
              setState(() {
                ThemeManager.toggleTheme(); // Update the theme
              });
            },
          ),

          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () async {
              // Close WebSocket connection
              try {
                await channel.sink.close();
                            } catch (e) {
                debugPrint('Error closing WebSocket: $e');
              }

              // Clear user data
              SharedPreferences prefs = await SharedPreferences.getInstance();
              await prefs.remove('jwt_token');
              await prefs.remove('user_id');

              // Navigate to LoginPage
              if (mounted) {
                Navigator.pushAndRemoveUntil(
                  context,
                  MaterialPageRoute(builder: (context) => const LoginPage()),
                      (route) => false, // Remove all previous routes
                );
              }
            },
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: ValueListenableBuilder<List<Map<String, dynamic>>>(
          valueListenable: devicesNotifier,
          builder: (context, devices, _) {
            return DeviceGrid(
              devices: devices,
              onDeviceTap: _showDevicePopup,
            );
          },
        ),
      ),
    );
  }

  @override
  void dispose() {
    channel.sink.close();

    devicesNotifier.dispose();
    super.dispose();
  }
}
