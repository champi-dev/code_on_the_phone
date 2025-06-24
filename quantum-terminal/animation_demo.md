# Quantum Terminal Animation Demo

Since we're in a non-graphical environment, here's what you would see when running the quantum terminal with animations:

## 🎬 Animation Demonstrations

### 1. Matrix Rain Effect (`ls`)
```
$ ls -la
```
**Visual**: Hundreds of green particles falling from the top of the screen like digital rain, each particle flickering with varying brightness. The particles spell out random characters as they fall, creating the iconic Matrix effect.

### 2. Wormhole Portal (`cd /home`)
```
$ cd /home
```
**Visual**: A swirling vortex of blue and purple particles spiraling outward from the cursor position, creating a portal effect that seems to pull space-time around it.

### 3. DNA Helix (`git status`)
```
$ git status
```
**Visual**: Two intertwined strands of particles forming a double helix, with colors representing DNA bases:
- Green (Adenine)
- Red (Thymine)  
- Blue (Cytosine)
- Yellow (Guanine)

### 4. Quantum Explosion (`rm -rf`)
```
$ rm -rf temp/
```
**Visual**: A massive explosion of 500+ particles in red, orange, and yellow, bursting outward from the cursor with high velocity, simulating a fiery explosion.

### 5. Glitch Effect (`sudo apt update`)
```
$ sudo apt update
```
**Visual**: Particles separating into RGB channels (pure red, green, and blue), jittering and teleporting randomly around the text, creating a digital glitch effect.

## 🎮 Interactive Features

- **Mouse Interaction**: Moving the mouse spawns particle trails
- **Particle Physics**: Gravity, drag, and turbulence affect all particles
- **Secondary Spawning**: Large particles spawn smaller ones on impact
- **Energy Glow**: Particles pulse with quantum energy
- **Smooth 60 FPS**: Hardware-accelerated rendering

## 🚀 Running the Demo

To see these animations on a system with graphics:

```bash
# Build the terminal
cd quantum-terminal
make clean && make

# Run the terminal
./build/quantum-terminal

# Try these commands to trigger animations:
ls                    # Matrix rain
cd Documents          # Wormhole portal  
git status           # DNA helix
sudo whoami          # Glitch effect
make                 # Particle fountain
history              # Time warp
python3              # Neural network
vim test.txt         # Cosmic rays
ssh user@host        # Quantum tunnel
```

## 📹 Animation Timeline

Each animation has specific behaviors:

- **0-1s**: Initial particle spawn with burst effect
- **1-3s**: Main animation loop with physics simulation
- **3-5s**: Particles fade out and despawn
- **Continuous**: Some effects (like Matrix rain) loop continuously

## 🎨 Visual Details

The particle system renders:
- Up to 10,000 simultaneous particles
- Additive blending for glow effects
- Billboard quads that always face the camera
- Smooth color gradients and transitions
- Real-time physics simulation
- Energy-based brightness modulation

To experience the full visual spectacle, run the quantum terminal on a system with OpenGL or Metal support!