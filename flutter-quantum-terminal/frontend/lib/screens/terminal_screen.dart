import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:xterm/xterm.dart';
import '../widgets/quantum_animation_overlay.dart';
import '../widgets/terminal_3d_effects.dart';
import '../widgets/connection_status.dart';
import '../services/terminal_service.dart';
import '../services/animation_service.dart';

class TerminalScreen extends StatefulWidget {
  const TerminalScreen({super.key});

  @override
  State<TerminalScreen> createState() => _TerminalScreenState();
}

class _TerminalScreenState extends State<TerminalScreen> {
  late Terminal terminal;
  late TerminalController terminalController;
  String currentCommand = '';

  @override
  void initState() {
    super.initState();
    terminal = Terminal(
      maxLines: 10000,
    );
    terminalController = TerminalController();
    
    // Listen for terminal input for 3D effects
    terminal.onInput = (String input) {
      setState(() {
        if (input == '\r' || input == '\n') {
          // Trigger 3D effect based on command
          context.read<AnimationService>().triggerAnimation(currentCommand);
          currentCommand = '';
        } else if (input == '\x7f') {
          // Backspace
          if (currentCommand.isNotEmpty) {
            currentCommand = currentCommand.substring(0, currentCommand.length - 1);
          }
        } else {
          currentCommand += input;
        }
      });
    };
    
    // Connect to backend
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final terminalService = context.read<TerminalService>();
      terminalService.connect(terminal);
    });
  }

  @override
  void dispose() {
    context.read<TerminalService>().disconnect();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0A0A0F),
      body: Stack(
        children: [
          // Quantum background
          Container(
            decoration: BoxDecoration(
              gradient: RadialGradient(
                center: Alignment.center,
                radius: 1.5,
                colors: [
                  const Color(0xFF0A0A0F),
                  const Color(0xFF000000),
                ],
              ),
            ),
          ),
          
          // Terminal
          Column(
            children: [
              // App bar
              Container(
                height: 40,
                decoration: BoxDecoration(
                  color: const Color(0xFF1A1A1F),
                  boxShadow: [
                    BoxShadow(
                      color: Colors.cyan.withOpacity(0.3),
                      blurRadius: 10,
                      offset: const Offset(0, 2),
                    ),
                  ],
                ),
                child: Row(
                  children: [
                    const SizedBox(width: 16),
                    Text(
                      'Quantum Terminal',
                      style: TextStyle(
                        color: Colors.cyan,
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                        shadows: [
                          Shadow(
                            color: Colors.cyan.withOpacity(0.8),
                            blurRadius: 10,
                          ),
                        ],
                      ),
                    ),
                    const Spacer(),
                    const ConnectionStatus(),
                    const SizedBox(width: 8),
                    // Connect to droplet button
                    Consumer<TerminalService>(
                      builder: (context, service, _) {
                        return TextButton.icon(
                          icon: Icon(
                            service.isRemote ? Icons.cloud_done : Icons.cloud_off,
                            size: 16,
                          ),
                          label: Text(
                            service.isRemote ? 'Disconnect' : 'Connect to Droplet',
                            style: const TextStyle(fontSize: 12),
                          ),
                          onPressed: () {
                            if (service.isRemote) {
                              service.connectLocal();
                            } else {
                              _showDropletDialog(context);
                            }
                          },
                        );
                      },
                    ),
                    const SizedBox(width: 16),
                  ],
                ),
              ),
              
              // Terminal view
              Expanded(
                child: Container(
                  margin: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: const Color(0xFF0A0A0F).withOpacity(0.9),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: Colors.cyan.withOpacity(0.3),
                      width: 1,
                    ),
                    boxShadow: [
                      BoxShadow(
                        color: Colors.cyan.withOpacity(0.2),
                        blurRadius: 20,
                        spreadRadius: -5,
                      ),
                    ],
                  ),
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(8),
                    child: TerminalView(
                      terminal,
                      controller: terminalController,
                      theme: TerminalTheme(
                        cursor: const Color(0xFF00FF00),
                        selection: const Color(0xFF00FF00).withOpacity(0.3),
                        foreground: const Color(0xFF00FF00),
                        background: const Color(0xFF0A0A0F),
                        black: const Color(0xFF000000),
                        red: const Color(0xFFFF0040),
                        green: const Color(0xFF00FF00),
                        yellow: const Color(0xFFFFFF00),
                        blue: const Color(0xFF00FFFF),
                        magenta: const Color(0xFFFF00FF),
                        cyan: const Color(0xFF00FFFF),
                        white: const Color(0xFFFFFFFF),
                        brightBlack: const Color(0xFF808080),
                        brightRed: const Color(0xFFFF0080),
                        brightGreen: const Color(0xFF00FF80),
                        brightYellow: const Color(0xFFFFFF80),
                        brightBlue: const Color(0xFF80FFFF),
                        brightMagenta: const Color(0xFFFF80FF),
                        brightCyan: const Color(0xFF80FFFF),
                        brightWhite: const Color(0xFFFFFFFF),
                      ),
                      padding: const EdgeInsets.all(16),
                      alwaysShowCursor: true,
                      autofocus: true,
                    ),
                  ),
                ),
              ),
            ],
          ),
          
          // Quantum animation overlay
          const QuantumAnimationOverlay(),
          
          // Award-winning 3D effects overlay
          Terminal3DEffects(
            currentCommand: currentCommand,
            onEffectComplete: () {
              // Effect completed callback if needed
            },
          ),
        ],
      ),
    );
  }

  void _showDropletDialog(BuildContext context) {
    final hostController = TextEditingController();
    final usernameController = TextEditingController(text: 'root');
    
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Connect to Digital Ocean Droplet'),
        backgroundColor: const Color(0xFF1A1A1F),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: hostController,
              decoration: const InputDecoration(
                labelText: 'Droplet IP Address',
                hintText: '192.168.1.1',
                prefixIcon: Icon(Icons.dns),
              ),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: usernameController,
              decoration: const InputDecoration(
                labelText: 'Username',
                prefixIcon: Icon(Icons.person),
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Make sure your SSH key is configured in the backend',
              style: TextStyle(
                fontSize: 12,
                color: Colors.white.withOpacity(0.6),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              context.read<TerminalService>().connectToDroplet(
                hostController.text,
                usernameController.text,
              );
              Navigator.pop(context);
            },
            child: const Text('Connect'),
          ),
        ],
      ),
    );
  }
}