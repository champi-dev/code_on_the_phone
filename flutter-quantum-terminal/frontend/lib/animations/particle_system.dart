import 'package:flutter/material.dart';
import 'dart:math' as math;

class Particle {
  Offset position;
  Offset velocity;
  Color color;
  double size;
  double lifetime;
  double age = 0;
  double alpha = 1.0;
  double gravity;
  double spin;
  String? character;
  bool trail;
  bool flicker;

  Particle({
    required this.position,
    required this.velocity,
    required this.color,
    required this.size,
    required this.lifetime,
    this.gravity = 0,
    this.spin = 0,
    this.character,
    this.trail = false,
    this.flicker = false,
  });

  bool get isDead => age >= lifetime;

  void update(double deltaTime) {
    age += deltaTime;
    
    // Update position
    position = position.translate(
      velocity.dx * deltaTime,
      velocity.dy * deltaTime,
    );
    
    // Apply gravity
    if (gravity > 0) {
      velocity = velocity.translate(0, gravity * deltaTime);
    }
    
    // Apply spin
    if (spin > 0) {
      final angle = spin * deltaTime;
      final cos = math.cos(angle);
      final sin = math.sin(angle);
      velocity = Offset(
        velocity.dx * cos - velocity.dy * sin,
        velocity.dx * sin + velocity.dy * cos,
      );
    }
    
    // Update alpha
    alpha = 1.0 - (age / lifetime);
    if (alpha < 0) alpha = 0;
    
    // Flicker effect
    if (flicker && math.Random().nextDouble() < 0.1) {
      alpha *= math.Random().nextDouble();
    }
  }
}