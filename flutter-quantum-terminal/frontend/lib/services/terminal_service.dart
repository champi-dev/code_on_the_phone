import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:xterm/xterm.dart';
import 'animation_service.dart';

class TerminalService extends ChangeNotifier {
  WebSocketChannel? _channel;
  Terminal? _terminal;
  AnimationService? _animationService;
  
  bool _isConnected = false;
  bool _isRemote = false;
  String _status = 'Disconnected';
  
  bool get isConnected => _isConnected;
  bool get isRemote => _isRemote;
  String get status => _status;

  void setAnimationService(AnimationService service) {
    _animationService = service;
  }

  void connect(Terminal terminal) async {
    _terminal = terminal;
    
    try {
      _status = 'Connecting...';
      notifyListeners();
      
      // Connect to Go backend
      final wsUrl = kIsWeb 
          ? 'ws://localhost:8080/ws'
          : 'ws://localhost:8080/ws'; // Change for production
      
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
      
      // Listen for messages from backend
      _channel!.stream.listen(
        _handleMessage,
        onError: (error) {
          _status = 'Error: $error';
          _isConnected = false;
          notifyListeners();
        },
        onDone: () {
          _status = 'Disconnected';
          _isConnected = false;
          notifyListeners();
        },
      );
      
      // Set up terminal input handler
      _terminal!.onInput = (input) {
        if (_isConnected) {
          _sendMessage({
            'type': 'input',
            'data': input,
          });
        }
      };
      
      // Set up terminal resize handler
      _terminal!.onResize = (width, height, pixelWidth, pixelHeight) {
        if (_isConnected) {
          _sendMessage({
            'type': 'resize',
            'cols': width,
            'rows': height,
          });
        }
      };
      
      _isConnected = true;
      _status = 'Connected (Local)';
      notifyListeners();
      
    } catch (e) {
      _status = 'Failed to connect: $e';
      _isConnected = false;
      notifyListeners();
    }
  }

  void disconnect() {
    _channel?.sink.close();
    _channel = null;
    _isConnected = false;
    _status = 'Disconnected';
    notifyListeners();
  }

  void connectToDroplet(String host, String username) {
    if (!_isConnected) return;
    
    _sendMessage({
      'type': 'connect',
      'target': 'remote',
      'host': host,
      'username': username,
    });
    
    _status = 'Connecting to droplet...';
    notifyListeners();
  }

  void connectLocal() {
    if (!_isConnected) return;
    
    _sendMessage({
      'type': 'connect',
      'target': 'local',
    });
    
    _isRemote = false;
    _status = 'Connected (Local)';
    notifyListeners();
  }

  void _sendMessage(Map<String, dynamic> message) {
    if (_channel != null) {
      _channel!.sink.add(jsonEncode(message));
    }
  }

  void _handleMessage(dynamic message) {
    try {
      final data = jsonDecode(message);
      
      switch (data['type']) {
        case 'output':
          // Write output to terminal
          _terminal?.write(data['data']);
          break;
          
        case 'animation':
          // Trigger animation
          _animationService?.triggerAnimation(
            AnimationType.values.firstWhere(
              (e) => e.toString().split('.').last == data['animation'],
            ),
            data['x'] ?? 0,
            data['y'] ?? 0,
          );
          break;
          
        case 'connected':
          _isRemote = data['remote'] ?? false;
          _status = _isRemote ? 'Connected (Droplet)' : 'Connected (Local)';
          notifyListeners();
          break;
          
        case 'error':
          _status = 'Error: ${data['message']}';
          notifyListeners();
          break;
      }
    } catch (e) {
      debugPrint('Error handling message: $e');
    }
  }
}