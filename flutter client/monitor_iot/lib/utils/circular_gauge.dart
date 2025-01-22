import 'package:syncfusion_flutter_gauges/gauges.dart';
import 'package:flutter/material.dart';


class CircularGaugeWithIcon extends StatelessWidget {


  final double value;       // Current reading, e.g. 25.6
  final double minValue;    // Minimum scale, e.g. 0
  final double maxValue;    // Maximum scale, e.g. 100
  final String unit;        // Unit label, e.g. '°C' or '%'
  final IconData icon;      // Icon to display in the center
  final Color rangeColor;   // Color for the progress arc

  const CircularGaugeWithIcon({
    Key? key,
    required this.value,
    required this.minValue,
    required this.maxValue,
    required this.unit,
    required this.icon,
    this.rangeColor = Colors.blue,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // Calculate range pointer value (clamped within min-max)
    final double clampedValue = value < minValue
        ? minValue
        : (value > maxValue ? maxValue : value);

    return SizedBox(
      width: 200,
      height: 200,
      child: SfRadialGauge(
        axes: <RadialAxis>[
          RadialAxis(
            minimum: minValue,
            maximum: maxValue,
            showLabels: false,
            showTicks: false,
            showAxisLine: false,
            ranges: <GaugeRange>[
              GaugeRange(
                startValue: minValue,
                endValue: maxValue,
                color: rangeColor.withOpacity(0.2),
                startWidth: 15,
                endWidth: 15,
              ),
            ],
            pointers: <GaugePointer>[
              RangePointer(
                value: clampedValue,
                color: rangeColor,
                width: 15,
                cornerStyle: CornerStyle.bothCurve,
              ),
            ],
            annotations: <GaugeAnnotation>[
              GaugeAnnotation(
                angle: 90, // Places annotation in the center
                positionFactor: 0.0,
                widget: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(icon, size: 30, color: Colors.grey[700]),
                    const SizedBox(height: 4),
                    Text(
                      '${clampedValue.toStringAsFixed(1)} $unit',
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
