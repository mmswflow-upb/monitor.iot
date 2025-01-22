import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import '../utils/circular_gauge.dart';

class DevicePopupDialog extends StatefulWidget {
  final ValueNotifier<Map<String, dynamic>> deviceNotifier;

  // Current color for an RGB lamp
  final Color currentColor;

  // Called whenever a new color is selected
  final ValueChanged<Color> onColorChanged;

  // Called when user clicks "Set Color" or any final action for color
  final VoidCallback onSetColorPressed;

  // Called when camera or device "active" state toggles
  final ValueChanged<bool> onActiveChanged;

  const DevicePopupDialog({
    Key? key,
    required this.deviceNotifier,
    required this.currentColor,
    required this.onColorChanged,
    required this.onSetColorPressed,
    required this.onActiveChanged,
  }) : super(key: key);

  @override
  State<DevicePopupDialog> createState() => _DevicePopupDialogState();
}

class _DevicePopupDialogState extends State<DevicePopupDialog> {
  @override
  void initState() {
    super.initState();
    // We could add any initialization logic here if needed
  }

  @override
  Widget build(BuildContext context) {
    // We use ValueListenableBuilder so that whenever deviceNotifier.value changes,
    // only this popup's contents rebuild, not the entire UI or the popup itself.
    return ValueListenableBuilder<Map<String, dynamic>>(
      valueListenable: widget.deviceNotifier,
      builder: (context, device, _) {
        final String deviceType = device['deviceType'] ?? 'Unknown';
        final String deviceName = device['deviceName'] ?? 'Unnamed Device';

        return AlertDialog(
          title: Text('$deviceName ($deviceType)'),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                // Show different content based on device type:
                if (deviceType == 'RGB-Lamp') _buildRgbLampContent(device),
                if (deviceType == 'Camera') _buildCameraContent(device),
                if (deviceType == 'Temperature-Humidity-Sensor') _buildTempSensorContent(device),
                // Add more if-conditions for other device types...
              ],
            ),
          ),
          actions: [
            // Example action: Close if user wants to manually close
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Close'),
            ),
          ],
        );
      },
    );
  }

  /// Builds content for an RGB Lamp device
  Widget _buildRgbLampContent(Map<String, dynamic> device) {
    return StatefulBuilder(
      builder: (context, setState) {
        // Extract the current color dynamically from the device data
        Color selectedColor = Color.fromRGBO(
          device['data']['r'] ?? 0,
          device['data']['g'] ?? 0,
          device['data']['b'] ?? 0,
          1.0,
        );

        return Column(
          children: [
            // Label for the color picker
            const Text(
              'Pick a Color for the Lamp:',
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),

            // Current color preview
            Container(
              width: 50,
              height: 50,
              decoration: BoxDecoration(
                color: selectedColor, // Display the current color
                shape: BoxShape.circle,
                border: Border.all(color: Colors.black, width: 1),
              ),
            ),
            const SizedBox(height: 16),

            // ColorPicker for selecting a color
            ColorPicker(
              pickerColor: selectedColor, // Use the latest selected color
              onColorChanged: (color) {
                // Update the local color
                setState(() {
                  selectedColor = color;
                });

                // Call the parent `onColorChanged` to notify updates
                widget.onColorChanged(color);
              },
              pickerAreaHeightPercent: 0.7, // Adjust picker height
            ),
            const SizedBox(height: 16),

            // Set Color button
            ElevatedButton(
              onPressed: () {
                // Call the parent `onSetColorPressed` to finalize the update
                widget.onSetColorPressed();
              },
              child: const Text('Set Color'),
            ),
          ],
        );
      },
    );
  }










  /// Builds content for a Camera device
  Widget _buildCameraContent(Map<String, dynamic> device) {
    // Example: We have 'data' with 'binaryFrame' as base64 image
    final bool isActive = device['data']['active'] ?? false;
    final String base64Image = device['data']['binaryFrame'] ?? '';

    return Column(
      children: [
        // Toggle camera on/off
        SwitchListTile(
          title: const Text('Camera Active'),
          value: isActive,
          onChanged: (newValue) {
            widget.onActiveChanged(newValue);
          },
        ),
        const SizedBox(height: 8),
        if (base64Image.isNotEmpty)
        // We'll decode the base64 string into a Dart `Uint8List` and display
          Image.memory(
            _decodeBase64(base64Image),
            width: 200,
            height: 200,
            fit: BoxFit.contain,
          )
        else
          const Text('No camera feed'),
      ],
    );
  }

  /// Builds content for a Temperature-Humidity-Sensor
  Widget _buildTempSensorContent(Map<String, dynamic> device) {
    final double? temperature = device['data']['temperature']?.toDouble();
    final double? humidity    = device['data']['humidity']?.toDouble();

    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        // Temperature gauge
        CircularGaugeWithIcon(
          value: temperature ?? 0.0,
          minValue: -10,
          maxValue: 50,
          unit: '°C',
          icon: Icons.thermostat,
          rangeColor: Colors.red,
        ),
        // Humidity gauge
        CircularGaugeWithIcon(
          value: humidity ?? 0.0,
          minValue: 0,
          maxValue: 100,
          unit: '%',
          icon: Icons.water_drop, // or Icons.opacity for older Flutter versions
          rangeColor: Colors.blue,
        ),
      ],
    );
  }


  /// Decodes a base64 string to a Uint8List
  Uint8List _decodeBase64(String base64String) {
    return base64.decode(base64String);
  }
}

