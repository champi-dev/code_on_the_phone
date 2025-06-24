import 'dart:math' as math;
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_gl/flutter_gl.dart';
import 'package:vector_math/vector_math.dart' as vec;

class Terminal3DEffects extends StatefulWidget {
  final String currentCommand;
  final VoidCallback onEffectComplete;

  const Terminal3DEffects({
    Key? key,
    required this.currentCommand,
    required this.onEffectComplete,
  }) : super(key: key);

  @override
  State<Terminal3DEffects> createState() => _Terminal3DEffectsState();
}

class _Terminal3DEffectsState extends State<Terminal3DEffects>
    with TickerProviderStateMixin {
  late FlutterGlPlugin flutterGl;
  late AnimationController _animationController;
  late AnimationController _rotationController;
  
  String activeEffect = '';
  double time = 0.0;
  
  // Galaxy system variables
  List<vec.Vector3> galaxyPositions = [];
  List<Color> galaxyColors = [];
  
  // Quantum field variables
  List<List<vec.Vector3>> quantumField = [];
  
  // Neural network variables
  List<vec.Vector3> neuralNodes = [];
  List<List<int>> neuralConnections = [];
  
  // Matrix rain variables
  List<MatrixDrop> matrixDrops = [];
  
  // Holographic display variables
  List<HologramPlane> hologramPlanes = [];

  @override
  void initState() {
    super.initState();
    
    _animationController = AnimationController(
      duration: const Duration(seconds: 20),
      vsync: this,
    )..repeat();
    
    _rotationController = AnimationController(
      duration: const Duration(seconds: 10),
      vsync: this,
    )..repeat();
    
    _initializeEffectSystems();
    _detectCommandEffect();
    
    _animationController.addListener(() {
      setState(() {
        time = _animationController.value * math.pi * 2;
      });
    });
  }

  void _initializeEffectSystems() {
    // Initialize galaxy system
    final random = math.Random();
    for (int i = 0; i < 5000; i++) {
      final angle = random.nextDouble() * math.pi * 2;
      final radius = random.nextDouble() * 300;
      final spinAngle = radius * 0.005;
      
      galaxyPositions.add(vec.Vector3(
        math.cos(angle + spinAngle) * radius,
        (random.nextDouble() - 0.5) * 50 * math.exp(-radius / 300),
        math.sin(angle + spinAngle) * radius,
      ));
      
      final hue = 0.6 + (radius / 300) * 0.4;
      galaxyColors.add(HSVColor.fromAHSV(1.0, hue * 360, 0.8, 0.6).toColor());
    }
    
    // Initialize quantum field
    for (int i = 0; i < 32; i++) {
      List<vec.Vector3> row = [];
      for (int j = 0; j < 32; j++) {
        row.add(vec.Vector3(
          (i - 16) * 20.0,
          0,
          (j - 16) * 20.0,
        ));
      }
      quantumField.add(row);
    }
    
    // Initialize neural network
    for (int i = 0; i < 50; i++) {
      neuralNodes.add(vec.Vector3(
        (random.nextDouble() - 0.5) * 200,
        (random.nextDouble() - 0.5) * 200,
        (random.nextDouble() - 0.5) * 200,
      ));
    }
    
    // Create neural connections
    for (int i = 0; i < neuralNodes.length; i++) {
      for (int j = i + 1; j < neuralNodes.length; j++) {
        if (random.nextDouble() < 0.1) {
          neuralConnections.add([i, j]);
        }
      }
    }
    
    // Initialize matrix rain
    for (int i = 0; i < 50; i++) {
      matrixDrops.add(MatrixDrop(
        x: (i - 25) * 10.0,
        y: random.nextDouble() * 400,
        speed: 50 + random.nextDouble() * 50,
        characters: _generateMatrixCharacters(),
      ));
    }
    
    // Initialize holographic display
    for (int i = 0; i < 5; i++) {
      final angle = i * math.pi * 2 / 5;
      hologramPlanes.add(HologramPlane(
        position: vec.Vector3(
          math.cos(angle) * 150,
          math.sin(i * 2) * 20,
          math.sin(angle) * 150,
        ),
        rotation: angle,
      ));
    }
  }

  List<String> _generateMatrixCharacters() {
    final chars = '0123456789ABCDEF日本語中文한글';
    final random = math.Random();
    return List.generate(20, (_) => chars[random.nextInt(chars.length)]);
  }

  void _detectCommandEffect() {
    final cmd = widget.currentCommand.toLowerCase();
    
    if (cmd.contains('git')) {
      activeEffect = 'galaxy';
    } else if (cmd.contains('npm') || cmd.contains('node')) {
      activeEffect = 'quantum';
    } else if (cmd.contains('ssh') || cmd.contains('connect')) {
      activeEffect = 'hologram';
    } else if (cmd.contains('python') || cmd.contains('ai')) {
      activeEffect = 'neural';
    } else if (cmd.contains('hack') || cmd.contains('sudo')) {
      activeEffect = 'matrix';
    } else if (cmd == 'clear') {
      activeEffect = '';
    }
  }

  @override
  Widget build(BuildContext context) {
    if (activeEffect.isEmpty) {
      return const SizedBox.shrink();
    }
    
    return AnimatedBuilder(
      animation: _animationController,
      builder: (context, child) {
        return CustomPaint(
          size: Size.infinite,
          painter: _getEffectPainter(),
        );
      },
    );
  }

  CustomPainter _getEffectPainter() {
    switch (activeEffect) {
      case 'galaxy':
        return GalaxyPainter(
          positions: galaxyPositions,
          colors: galaxyColors,
          time: time,
          rotation: _rotationController.value * math.pi * 2,
        );
      case 'quantum':
        return QuantumFieldPainter(
          field: quantumField,
          time: time,
        );
      case 'neural':
        return NeuralNetworkPainter(
          nodes: neuralNodes,
          connections: neuralConnections,
          time: time,
        );
      case 'matrix':
        return MatrixRainPainter(
          drops: matrixDrops,
          time: time,
        );
      case 'hologram':
        return HologramPainter(
          planes: hologramPlanes,
          time: time,
          rotation: _rotationController.value * math.pi * 2,
        );
      default:
        return GalaxyPainter(
          positions: galaxyPositions,
          colors: galaxyColors,
          time: time,
          rotation: 0,
        );
    }
  }

  @override
  void dispose() {
    _animationController.dispose();
    _rotationController.dispose();
    super.dispose();
  }
}

class GalaxyPainter extends CustomPainter {
  final List<vec.Vector3> positions;
  final List<Color> colors;
  final double time;
  final double rotation;

  GalaxyPainter({
    required this.positions,
    required this.colors,
    required this.time,
    required this.rotation,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paint = Paint()..style = PaintingStyle.fill;
    
    for (int i = 0; i < positions.length; i++) {
      final pos = positions[i];
      final rotatedX = pos.x * math.cos(rotation) - pos.z * math.sin(rotation);
      final rotatedZ = pos.x * math.sin(rotation) + pos.z * math.cos(rotation);
      
      final screenX = center.dx + rotatedX;
      final screenY = center.dy + pos.y + math.sin(time + rotatedZ * 0.01) * 5;
      
      final depth = (rotatedZ + 300) / 600;
      final radius = 1 + depth * 2;
      
      paint.color = colors[i].withOpacity(0.8 * depth);
      paint.maskFilter = const MaskFilter.blur(BlurStyle.normal, 2);
      
      canvas.drawCircle(Offset(screenX, screenY), radius, paint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}

class QuantumFieldPainter extends CustomPainter {
  final List<List<vec.Vector3>> field;
  final double time;

  QuantumFieldPainter({
    required this.field,
    required this.time,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;
    
    for (int i = 0; i < field.length - 1; i++) {
      for (int j = 0; j < field[i].length - 1; j++) {
        final p1 = field[i][j];
        final p2 = field[i + 1][j];
        final p3 = field[i][j + 1];
        
        final wave1 = math.sin(p1.x * 0.02 + time) * math.cos(p1.z * 0.02 + time) * 20;
        final wave2 = math.sin(p2.x * 0.02 + time) * math.cos(p2.z * 0.02 + time) * 20;
        final wave3 = math.sin(p3.x * 0.02 + time) * math.cos(p3.z * 0.02 + time) * 20;
        
        final screen1 = Offset(center.dx + p1.x, center.dy + p1.z + wave1);
        final screen2 = Offset(center.dx + p2.x, center.dy + p2.z + wave2);
        final screen3 = Offset(center.dx + p3.x, center.dy + p3.z + wave3);
        
        final elevation = (wave1 + wave2 + wave3) / 3;
        final hue = 0.6 + elevation / 40;
        paint.color = HSVColor.fromAHSV(0.6, hue * 360, 0.8, 0.7).toColor();
        
        canvas.drawLine(screen1, screen2, paint);
        canvas.drawLine(screen1, screen3, paint);
      }
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}

class NeuralNetworkPainter extends CustomPainter {
  final List<vec.Vector3> nodes;
  final List<List<int>> connections;
  final double time;

  NeuralNetworkPainter({
    required this.nodes,
    required this.connections,
    required this.time,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final nodePaint = Paint()..style = PaintingStyle.fill;
    final connectionPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;
    
    // Draw connections
    for (final conn in connections) {
      final node1 = nodes[conn[0]];
      final node2 = nodes[conn[1]];
      
      final screen1 = Offset(center.dx + node1.x, center.dy + node1.y);
      final screen2 = Offset(center.dx + node2.x, center.dy + node2.y);
      
      final pulse = math.sin(time * 5 + conn[0] * 0.3) * 0.5 + 0.5;
      connectionPaint.color = Colors.greenAccent.withOpacity(0.1 + pulse * 0.4);
      
      canvas.drawLine(screen1, screen2, connectionPaint);
    }
    
    // Draw nodes
    for (int i = 0; i < nodes.length; i++) {
      final node = nodes[i];
      final screen = Offset(center.dx + node.x, center.dy + node.y);
      
      final pulse = math.sin(time * 3 + i * 0.5) * 0.5 + 0.5;
      final radius = 3 + pulse * 2;
      
      nodePaint.color = Colors.greenAccent.withOpacity(0.8);
      nodePaint.maskFilter = MaskFilter.blur(BlurStyle.normal, pulse * 3);
      
      canvas.drawCircle(screen, radius, nodePaint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}

class MatrixRainPainter extends CustomPainter {
  final List<MatrixDrop> drops;
  final double time;

  MatrixRainPainter({
    required this.drops,
    required this.time,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final textPainter = TextPainter(
      textDirection: TextDirection.ltr,
    );
    
    for (final drop in drops) {
      drop.update(time, size.height);
      
      for (int i = 0; i < drop.characters.length; i++) {
        final y = drop.y - i * 20;
        if (y < 0 || y > size.height) continue;
        
        final opacity = 1.0 - (i / drop.characters.length);
        final color = Colors.green.withOpacity(opacity * 0.8);
        
        textPainter.text = TextSpan(
          text: drop.characters[i],
          style: TextStyle(
            color: color,
            fontSize: 16,
            fontFamily: 'monospace',
            shadows: [
              Shadow(
                color: color,
                blurRadius: 10,
              ),
            ],
          ),
        );
        
        textPainter.layout();
        textPainter.paint(
          canvas,
          Offset(center.dx + drop.x - textPainter.width / 2, y),
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}

class HologramPainter extends CustomPainter {
  final List<HologramPlane> planes;
  final double time;
  final double rotation;

  HologramPainter({
    required this.planes,
    required this.time,
    required this.rotation,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paint = Paint()
      ..style = PaintingStyle.fill
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 3);
    
    for (int i = 0; i < planes.length; i++) {
      final plane = planes[i];
      final rotatedX = plane.position.x * math.cos(rotation) - 
                      plane.position.z * math.sin(rotation);
      final rotatedZ = plane.position.x * math.sin(rotation) + 
                      plane.position.z * math.cos(rotation);
      
      final screenX = center.dx + rotatedX;
      final screenY = center.dy + plane.position.y + math.sin(time + i) * 20;
      
      final scanline = math.sin(screenY * 0.1 + time * 5) * 0.1 + 0.9;
      final glitch = math.sin(time * 20) > 0.98 ? 0.05 : 0;
      
      paint.color = Colors.cyanAccent.withOpacity(
        (0.5 + math.sin(rotatedX * 0.01 + time * 2) * 0.5) * scanline * (1 - glitch)
      );
      
      final rect = Rect.fromCenter(
        center: Offset(screenX, screenY),
        width: 100,
        height: 30,
      );
      
      canvas.drawRect(rect, paint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}

class MatrixDrop {
  double x;
  double y;
  final double speed;
  final List<String> characters;

  MatrixDrop({
    required this.x,
    required this.y,
    required this.speed,
    required this.characters,
  });

  void update(double time, double screenHeight) {
    y += speed * 0.016; // 60 FPS
    if (y > screenHeight + characters.length * 20) {
      y = -characters.length * 20;
    }
  }
}

class HologramPlane {
  final vec.Vector3 position;
  final double rotation;

  HologramPlane({
    required this.position,
    required this.rotation,
  });
}