import 'package:flutter/foundation.dart';

enum AnimationType {
  matrixRain,
  wormholePortal,
  quantumExplosion,
  dnaHelix,
  glitchText,
  neuralNetwork,
  cosmicRays,
  particleFountain,
  timeWarp,
  quantumTunnel,
}

class AnimationTrigger {
  final AnimationType type;
  final int x;
  final int y;
  final DateTime timestamp;

  AnimationTrigger({
    required this.type,
    required this.x,
    required this.y,
  }) : timestamp = DateTime.now();
}

class AnimationService extends ChangeNotifier {
  AnimationTrigger? _currentAnimation;
  
  AnimationTrigger? get currentAnimation => _currentAnimation;

  void triggerAnimation(AnimationType type, int x, int y) {
    _currentAnimation = AnimationTrigger(
      type: type,
      x: x,
      y: y,
    );
    notifyListeners();
    
    // Clear animation after a short delay
    Future.delayed(const Duration(milliseconds: 100), () {
      _currentAnimation = null;
      notifyListeners();
    });
  }
}