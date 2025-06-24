import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/terminal_service.dart';

class ConnectionStatus extends StatefulWidget {
  const ConnectionStatus({super.key});

  @override
  State<ConnectionStatus> createState() => _ConnectionStatusState();
}

class _ConnectionStatusState extends State<ConnectionStatus>
    with SingleTickerProviderStateMixin {
  late AnimationController _animationController;
  late Animation<double> _animation;

  @override
  void initState() {
    super.initState();
    _animationController = AnimationController(
      duration: const Duration(seconds: 2),
      vsync: this,
    )..repeat(reverse: true);
    
    _animation = Tween<double>(
      begin: 0.3,
      end: 1.0,
    ).animate(CurvedAnimation(
      parent: _animationController,
      curve: Curves.easeInOut,
    ));
  }

  @override
  void dispose() {
    _animationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<TerminalService>(
      builder: (context, service, _) {
        final isConnected = service.isConnected;
        final status = service.status;
        
        Color statusColor;
        IconData statusIcon;
        
        if (isConnected) {
          statusColor = service.isRemote ? Colors.blue : Colors.green;
          statusIcon = service.isRemote ? Icons.cloud : Icons.computer;
        } else if (status.startsWith('Connecting')) {
          statusColor = Colors.orange;
          statusIcon = Icons.sync;
        } else {
          statusColor = Colors.red;
          statusIcon = Icons.cloud_off;
        }
        
        return Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            AnimatedBuilder(
              animation: _animation,
              builder: (context, child) {
                return Icon(
                  statusIcon,
                  size: 16,
                  color: statusColor.withOpacity(
                    status.startsWith('Connecting') ? _animation.value : 1.0,
                  ),
                );
              },
            ),
            const SizedBox(width: 8),
            Text(
              status,
              style: TextStyle(
                fontSize: 12,
                color: statusColor,
              ),
            ),
          ],
        );
      },
    );
  }
}