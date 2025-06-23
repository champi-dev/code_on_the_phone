package com.quantum.terminal;

import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class QuantumTerminalActivity extends Activity {
    private QuantumGLSurfaceView glView;
    private QuantumKeyboard keyboard;
    
    static {
        System.loadLibrary("quantum-terminal");
    }
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Create main layout
        FrameLayout layout = new FrameLayout(this);
        
        // Create OpenGL view
        glView = new QuantumGLSurfaceView(this);
        layout.addView(glView);
        
        // Create virtual keyboard overlay
        keyboard = new QuantumKeyboard(this);
        layout.addView(keyboard);
        
        setContentView(layout);
        
        // Initialize native code
        nativeInit();
    }
    
    @Override
    protected void onPause() {
        super.onPause();
        glView.onPause();
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        glView.onResume();
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Send key to native code
        nativeKeyDown(keyCode, event.getUnicodeChar());
        return true;
    }
    
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeKeyUp(keyCode);
        return true;
    }
    
    // Native methods
    private native void nativeInit();
    private native void nativeResize(int width, int height);
    private native void nativeRender();
    private native void nativeUpdate(float deltaTime);
    private native void nativeTouch(int action, float x, float y);
    private native void nativeKeyDown(int keyCode, int unicode);
    private native void nativeKeyUp(int keyCode);
    
    // Custom GLSurfaceView
    class QuantumGLSurfaceView extends GLSurfaceView {
        private QuantumRenderer renderer;
        private long lastTime;
        
        public QuantumGLSurfaceView(Context context) {
            super(context);
            
            // Set OpenGL ES 2.0
            setEGLContextClientVersion(2);
            
            // Set renderer
            renderer = new QuantumRenderer();
            setRenderer(renderer);
            
            // Enable touch events
            setFocusableInTouchMode(true);
        }
        
        @Override
        public boolean onTouchEvent(MotionEvent event) {
            float x = event.getX();
            float y = event.getY();
            
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    nativeTouch(0, x, y);
                    break;
                case MotionEvent.ACTION_MOVE:
                    nativeTouch(1, x, y);
                    break;
                case MotionEvent.ACTION_UP:
                    nativeTouch(2, x, y);
                    break;
            }
            
            return true;
        }
    }
    
    // Renderer
    class QuantumRenderer implements GLSurfaceView.Renderer {
        private long lastTime = System.nanoTime();
        
        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            // Native init is called from activity onCreate
        }
        
        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            nativeResize(width, height);
        }
        
        @Override
        public void onDrawFrame(GL10 gl) {
            // Calculate delta time
            long currentTime = System.nanoTime();
            float deltaTime = (currentTime - lastTime) / 1000000000.0f;
            lastTime = currentTime;
            
            // Update and render
            nativeUpdate(deltaTime);
            nativeRender();
        }
    }
    
    // Virtual keyboard with special terminal keys
    class QuantumKeyboard extends android.widget.LinearLayout {
        public QuantumKeyboard(Context context) {
            super(context);
            setOrientation(HORIZONTAL);
            
            // Add special keys: ESC, CTRL, ALT, TAB, Arrow keys
            addSpecialKey("ESC", KeyEvent.KEYCODE_ESCAPE);
            addSpecialKey("CTRL", KeyEvent.KEYCODE_CTRL_LEFT);
            addSpecialKey("ALT", KeyEvent.KEYCODE_ALT_LEFT);
            addSpecialKey("TAB", KeyEvent.KEYCODE_TAB);
            addSpecialKey("↑", KeyEvent.KEYCODE_DPAD_UP);
            addSpecialKey("↓", KeyEvent.KEYCODE_DPAD_DOWN);
            addSpecialKey("←", KeyEvent.KEYCODE_DPAD_LEFT);
            addSpecialKey("→", KeyEvent.KEYCODE_DPAD_RIGHT);
        }
        
        private void addSpecialKey(String label, final int keyCode) {
            android.widget.Button button = new android.widget.Button(getContext());
            button.setText(label);
            button.setOnClickListener(v -> {
                nativeKeyDown(keyCode, 0);
                nativeKeyUp(keyCode);
            });
            addView(button);
        }
    }
}