import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'screens/terminal_screen.dart';
import 'services/terminal_service.dart';
import 'services/animation_service.dart';

void main() {
  runApp(const QuantumTerminalApp());
}

class QuantumTerminalApp extends StatelessWidget {
  const QuantumTerminalApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => TerminalService()),
        ChangeNotifierProvider(create: (_) => AnimationService()),
      ],
      child: MaterialApp(
        title: 'Quantum Terminal',
        theme: ThemeData.dark().copyWith(
          colorScheme: ColorScheme.dark(
            primary: Colors.cyan,
            secondary: Colors.purple,
            surface: const Color(0xFF0A0A0F),
            background: const Color(0xFF000000),
          ),
          scaffoldBackgroundColor: const Color(0xFF000000),
          fontFamily: 'JetBrainsMono',
        ),
        home: const TerminalScreen(),
        debugShowCheckedModeBanner: false,
      ),
    );
  }
}