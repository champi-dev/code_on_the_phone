import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'dart:math' as math;
import '../services/animation_service.dart';
import '../animations/particle_system.dart';

class QuantumAnimationOverlay extends StatefulWidget {
  const QuantumAnimationOverlay({super.key});

  @override
  State<QuantumAnimationOverlay> createState() => _QuantumAnimationOverlayState();
}

class _QuantumAnimationOverlayState extends State<QuantumAnimationOverlay>
    with TickerProviderStateMixin {
  late AnimationController _controller;
  final List<Particle> particles = [];

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      duration: const Duration(seconds: 1),
      vsync: this,
    )..repeat();

    // Listen for animation triggers
    context.read<AnimationService>().addListener(_onAnimationTrigger);
  }

  @override
  void dispose() {
    context.read<AnimationService>().removeListener(_onAnimationTrigger);
    _controller.dispose();
    super.dispose();
  }

  void _onAnimationTrigger() {
    final animationService = context.read<AnimationService>();
    final animation = animationService.currentAnimation;
    
    if (animation != null) {
      setState(() {
        _createAnimation(animation);
      });
    }
  }

  void _createAnimation(AnimationTrigger trigger) {
    switch (trigger.type) {
      case AnimationType.matrixRain:
        _createMatrixRain(trigger);
        break;
      case AnimationType.wormholePortal:
        _createWormholePortal(trigger);
        break;
      case AnimationType.quantumExplosion:
        _createQuantumExplosion(trigger);
        break;
      case AnimationType.dnaHelix:
        _createDNAHelix(trigger);
        break;
      case AnimationType.glitchText:
        _createGlitchEffect(trigger);
        break;
      case AnimationType.neuralNetwork:
        _createNeuralNetwork(trigger);
        break;
      case AnimationType.cosmicRays:
        _createCosmicRays(trigger);
        break;
      case AnimationType.particleFountain:
        _createParticleFountain(trigger);
        break;
      case AnimationType.timeWarp:
        _createTimeWarp(trigger);
        break;
      case AnimationType.quantumTunnel:
        _createQuantumTunnel(trigger);
        break;
    }
  }

  void _createMatrixRain(AnimationTrigger trigger) {
    final size = MediaQuery.of(context).size;
    final random = math.Random();
    
    for (int i = 0; i < 100; i++) {
      particles.add(Particle(
        position: Offset(
          random.nextDouble() * size.width,
          -random.nextDouble() * 200,
        ),
        velocity: Offset(0, 50 + random.nextDouble() * 100),
        color: Color.lerp(
          Colors.green,
          Colors.lightGreen,
          random.nextDouble(),
        )!.withOpacity(0.8),
        size: 2 + random.nextDouble() * 4,
        lifetime: 3 + random.nextDouble() * 2,
        character: String.fromCharCode(33 + random.nextInt(94)), // Random ASCII
      ));
    }
  }

  void _createWormholePortal(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 200; i++) {
      final angle = random.nextDouble() * math.pi * 2;
      final radius = random.nextDouble() * 100;
      final speed = 50 + random.nextDouble() * 100;
      
      particles.add(Particle(
        position: center,
        velocity: Offset(
          math.cos(angle) * speed,
          math.sin(angle) * speed,
        ),
        color: Color.lerp(
          Colors.blue,
          Colors.purple,
          random.nextDouble(),
        )!.withOpacity(0.8),
        size: 3 + random.nextDouble() * 3,
        lifetime: 2,
        spin: 5 + random.nextDouble() * 5,
      ));
    }
  }

  void _createQuantumExplosion(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 500; i++) {
      final angle = random.nextDouble() * math.pi * 2;
      final speed = 100 + random.nextDouble() * 300;
      
      Color color;
      final heat = random.nextDouble();
      if (heat < 0.3) {
        color = Colors.red;
      } else if (heat < 0.7) {
        color = Colors.orange;
      } else {
        color = Colors.yellow;
      }
      
      particles.add(Particle(
        position: center,
        velocity: Offset(
          math.cos(angle) * speed,
          math.sin(angle) * speed,
        ),
        color: color.withOpacity(0.9),
        size: 4 + random.nextDouble() * 4,
        lifetime: 1.5,
        gravity: 200,
      ));
    }
  }

  void _createDNAHelix(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    
    for (int i = 0; i < 100; i++) {
      final t = i / 100.0;
      final angle = t * math.pi * 8; // 4 full rotations
      
      // Base colors - ATCG
      final colors = [
        Colors.green,   // Adenine
        Colors.red,     // Thymine
        Colors.blue,    // Cytosine
        Colors.yellow,  // Guanine
      ];
      
      // First strand
      particles.add(Particle(
        position: Offset(
          center.dx + math.cos(angle) * 30,
          center.dy + (t - 0.5) * 200,
        ),
        velocity: const Offset(0, -50),
        color: colors[i % 4].withOpacity(0.9),
        size: 6,
        lifetime: 3,
      ));
      
      // Second strand (complementary)
      particles.add(Particle(
        position: Offset(
          center.dx - math.cos(angle) * 30,
          center.dy + (t - 0.5) * 200,
        ),
        velocity: const Offset(0, -50),
        color: colors[(i + 2) % 4].withOpacity(0.9), // Complementary base
        size: 6,
        lifetime: 3,
      ));
    }
  }

  void _createGlitchEffect(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 100; i++) {
      final channel = i % 3;
      Color color;
      switch (channel) {
        case 0:
          color = Colors.red;
          break;
        case 1:
          color = Colors.green;
          break;
        default:
          color = Colors.blue;
      }
      
      particles.add(Particle(
        position: Offset(
          center.dx + (random.nextDouble() - 0.5) * 100,
          center.dy + (random.nextDouble() - 0.5) * 50,
        ),
        velocity: Offset(
          (random.nextDouble() - 0.5) * 200,
          (random.nextDouble() - 0.5) * 200,
        ),
        color: color.withOpacity(0.8),
        size: 2 + random.nextDouble() * 4,
        lifetime: 0.5 + random.nextDouble() * 0.5,
        flicker: true,
      ));
    }
  }

  void _createNeuralNetwork(AnimationTrigger trigger) {
    // Implementation for neural network animation
    _createParticleFountain(trigger); // Placeholder
  }

  void _createCosmicRays(AnimationTrigger trigger) {
    final size = MediaQuery.of(context).size;
    final random = math.Random();
    
    for (int i = 0; i < 50; i++) {
      particles.add(Particle(
        position: Offset(
          random.nextDouble() * size.width,
          0,
        ),
        velocity: Offset(
          (random.nextDouble() - 0.5) * 100,
          200 + random.nextDouble() * 200,
        ),
        color: Colors.cyanAccent.withOpacity(0.9),
        size: 1 + random.nextDouble() * 2,
        lifetime: 2,
        trail: true,
      ));
    }
  }

  void _createParticleFountain(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 200; i++) {
      final angle = (random.nextDouble() - 0.5) * math.pi / 4; // Cone shape
      final speed = 100 + random.nextDouble() * 100;
      
      particles.add(Particle(
        position: center,
        velocity: Offset(
          math.sin(angle) * speed,
          -math.cos(angle) * speed,
        ),
        color: Color.lerp(
          Colors.cyan,
          Colors.purple,
          random.nextDouble(),
        )!.withOpacity(0.8),
        size: 3 + random.nextDouble() * 3,
        lifetime: 2,
        gravity: 100,
      ));
    }
  }

  void _createTimeWarp(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 150; i++) {
      final angle = random.nextDouble() * math.pi * 2;
      final radius = random.nextDouble() * 150;
      
      particles.add(Particle(
        position: Offset(
          center.dx + math.cos(angle) * radius,
          center.dy + math.sin(angle) * radius,
        ),
        velocity: Offset(
          -math.cos(angle) * 50,
          -math.sin(angle) * 50,
        ),
        color: Colors.deepPurple.withOpacity(0.7),
        size: 2 + random.nextDouble() * 4,
        lifetime: 2,
        spin: 10,
      ));
    }
  }

  void _createQuantumTunnel(AnimationTrigger trigger) {
    final center = Offset(trigger.x.toDouble(), trigger.y.toDouble());
    final random = math.Random();
    
    for (int i = 0; i < 200; i++) {
      final z = random.nextDouble();
      final angle = random.nextDouble() * math.pi * 2;
      final radius = 20 + z * 100;
      
      particles.add(Particle(
        position: Offset(
          center.dx + math.cos(angle) * radius,
          center.dy + math.sin(angle) * radius * 0.5,
        ),
        velocity: Offset(
          -radius * 0.5,
          0,
        ),
        color: Colors.tealAccent.withOpacity(0.8 - z * 0.5),
        size: 2 + (1 - z) * 4,
        lifetime: 2,
      ));
    }
  }

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: AnimatedBuilder(
        animation: _controller,
        builder: (context, child) {
          // Update particles
          final deltaTime = 1.0 / 60.0; // 60 FPS
          particles.removeWhere((particle) {
            particle.update(deltaTime);
            return particle.isDead;
          });

          return CustomPaint(
            size: MediaQuery.of(context).size,
            painter: ParticlePainter(particles),
          );
        },
      ),
    );
  }
}

class ParticlePainter extends CustomPainter {
  final List<Particle> particles;

  ParticlePainter(this.particles);

  @override
  void paint(Canvas canvas, Size size) {
    for (final particle in particles) {
      final paint = Paint()
        ..color = particle.color.withOpacity(particle.color.opacity * particle.alpha)
        ..style = PaintingStyle.fill;

      if (particle.trail) {
        // Draw trail
        paint.strokeWidth = particle.size;
        paint.style = PaintingStyle.stroke;
        canvas.drawLine(
          particle.position,
          particle.position - particle.velocity.scale(0.1, 0.1),
          paint,
        );
      } else if (particle.character != null) {
        // Draw character (for Matrix rain)
        final textPainter = TextPainter(
          text: TextSpan(
            text: particle.character,
            style: TextStyle(
              color: particle.color.withOpacity(particle.alpha),
              fontSize: particle.size * 3,
              fontFamily: 'JetBrainsMono',
            ),
          ),
          textDirection: TextDirection.ltr,
        );
        textPainter.layout();
        textPainter.paint(canvas, particle.position);
      } else {
        // Draw circle
        canvas.drawCircle(particle.position, particle.size, paint);
        
        // Add glow effect
        paint.color = particle.color.withOpacity(particle.alpha * 0.3);
        canvas.drawCircle(particle.position, particle.size * 2, paint);
      }
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}