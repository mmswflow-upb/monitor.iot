import 'package:flutter/material.dart';
import 'device_card.dart';

class DeviceGrid extends StatelessWidget {
  final List<Map<String, dynamic>> devices;
  final Function(Map<String, dynamic>) onDeviceTap;

  const DeviceGrid({
    super.key,
    required this.devices,
    required this.onDeviceTap,
  });

  @override
  Widget build(BuildContext context) {
    return GridView.builder(
      padding: const EdgeInsets.all(8.0),
      itemCount: devices.length,
      gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
        maxCrossAxisExtent: 320, // Maximum width of each card in dp
        crossAxisSpacing: 8.0,   // Space between columns
        mainAxisSpacing: 8.0,    // Space between rows
        childAspectRatio: 0.8,   // Adjust card's width-to-height ratio (lower = taller)
      ),
      itemBuilder: (context, index) {
        final device = devices[index];
        return DeviceCard(
          device: device,
          onTap: () => onDeviceTap(device),
        );
      },
    );
  }
}
