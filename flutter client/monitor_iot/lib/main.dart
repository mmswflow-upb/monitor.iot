import 'package:flutter/material.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart'; // Import dotenv package
import 'package:monitor_iot/pages/home_page.dart';
import 'theme_manager.dart'; // Import the theme manager

void main() async {
  await dotenv.load(fileName: ".env"); // Load environment variables
  runApp(const MonitorIoTApp());
}

class MonitorIoTApp extends StatelessWidget {
  const MonitorIoTApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<ThemeMode>(
      valueListenable: ThemeManager.themeNotifier,
      builder: (context, ThemeMode currentMode, child) {
        return MaterialApp(
          debugShowCheckedModeBanner: false, // Disables the "Debug" banner
          title: 'Monitor.IoT',
          theme: ThemeData.light(),
          darkTheme: ThemeData.dark(),
          themeMode: currentMode,
          home: const HomePage(),
        );
      },
    );
  }
}
