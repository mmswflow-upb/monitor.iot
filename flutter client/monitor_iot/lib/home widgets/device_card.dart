import 'package:flutter/material.dart';

class DeviceCard extends StatelessWidget {
  final Map<String, dynamic> device;
  final VoidCallback onTap;

  const DeviceCard({
    Key? key,
    required this.device,
    required this.onTap,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 5,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(8),
      ),
      child: GestureDetector(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(8.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              // Image/Icon based on device type
              if (device['deviceType'] == 'RGB-Lamp')
                Image.asset(
                  'assets/icon/RGB_Lamp.png',
                  width: 40,
                  height: 40,
                )
              else if (device['deviceType'] == 'Camera')
                const Icon(Icons.camera_alt, size: 40)
              else if (device['deviceType'] == 'Temperature-Humidity-Sensor')
                  Image.asset(
                    'assets/icon/temp_humidity.png',
                    width: 40,
                    height: 40,
                  )
                else
                  Image.asset(
                    'assets/icon/Camera.png',
                    width: 40,
                    height: 40,
                  ),
              const SizedBox(height: 8),
              Text(
                device['deviceName'] ?? 'Unknown Device',
                style: const TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.w500,
                ),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      ),
    );
  }
}
