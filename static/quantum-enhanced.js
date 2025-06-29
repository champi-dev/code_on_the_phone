// Enhanced Quantum Terminal Background Animation
class QuantumField {
    constructor() {
        this.canvas = document.getElementById('quantum-canvas');
        this.ctx = this.canvas.getContext('2d');
        this.particles = [];
        this.particleCount = 50; // Reduced for performance
        this.connections = [];
        this.mouse = { x: 0, y: 0 };
        this.time = 0;
        this.waveOffset = 0;
        
        // Quantum effects
        this.quantumBursts = [];
        this.energyWaves = [];
        this.entanglementPairs = [];
        
        this.init();
        this.animate();
        
        // Mouse/touch interaction
        const handleMove = (x, y) => {
            this.mouse.x = x;
            this.mouse.y = y;
            this.createQuantumBurst(x, y);
        };
        
        window.addEventListener('mousemove', (e) => handleMove(e.clientX, e.clientY));
        window.addEventListener('touchmove', (e) => {
            if (e.touches[0]) {
                handleMove(e.touches[0].clientX, e.touches[0].clientY);
            }
        });
        
        window.addEventListener('resize', () => this.resize());
    }
    
    init() {
        this.resize();
        
        // Create diverse particles
        for (let i = 0; i < this.particleCount; i++) {
            const angle = (i / this.particleCount) * Math.PI * 2;
            const radius = Math.random() * Math.min(this.canvas.width, this.canvas.height) * 0.4;
            
            this.particles.push({
                x: this.canvas.width / 2 + Math.cos(angle) * radius + (Math.random() - 0.5) * 200,
                y: this.canvas.height / 2 + Math.sin(angle) * radius + (Math.random() - 0.5) * 200,
                z: Math.random() * 1000,
                vx: (Math.random() - 0.5) * 1,
                vy: (Math.random() - 0.5) * 1,
                vz: (Math.random() - 0.5) * 2,
                phase: Math.random() * Math.PI * 2,
                frequency: 0.5 + Math.random() * 2,
                amplitude: 10 + Math.random() * 30,
                entangled: null,
                type: Math.random() < 0.3 ? 'quantum' : 'normal',
                color: this.getParticleColor(i),
                size: 2 + Math.random() * 3,
                brightness: 0.5 + Math.random() * 0.5
            });
        }
        
        // Create entanglements
        for (let i = 0; i < this.particleCount; i++) {
            if (Math.random() < 0.2) {
                const j = Math.floor(Math.random() * this.particleCount);
                if (i !== j) {
                    this.particles[i].entangled = j;
                    this.particles[j].entangled = i;
                    this.entanglementPairs.push([i, j]);
                }
            }
        }
    }
    
    getParticleColor(index) {
        const colors = [
            { r: 255, g: 255, b: 255 },   // White
            { r: 100, g: 149, b: 237 },   // Cornflower blue
            { r: 147, g: 112, b: 219 },   // Medium purple
            { r: 255, g: 182, b: 193 },   // Light pink
            { r: 176, g: 224, b: 230 },   // Powder blue
        ];
        
        const color = colors[index % colors.length];
        return `rgba(${color.r}, ${color.g}, ${color.b}, `;
    }
    
    resize() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
    }
    
    createQuantumBurst(x, y) {
        if (this.quantumBursts.length < 5) {
            this.quantumBursts.push({
                x, y,
                radius: 0,
                maxRadius: 200,
                opacity: 1,
                color: `hsl(${120 + Math.random() * 60}, 100%, 50%)`
            });
        }
    }
    
    createEnergyWave() {
        if (Math.random() < 0.02 && this.energyWaves.length < 3) {
            this.energyWaves.push({
                x: Math.random() * this.canvas.width,
                y: Math.random() * this.canvas.height,
                radius: 0,
                maxRadius: 500,
                opacity: 0.8,
                speed: 2 + Math.random() * 3
            });
        }
    }
    
    animate() {
        requestAnimationFrame(() => this.animate());
        
        this.time += 0.01;
        this.waveOffset += 0.02;
        
        // Clear canvas completely for better performance
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Create energy waves
        this.createEnergyWave();
        
        // Update and draw energy waves
        this.energyWaves = this.energyWaves.filter(wave => {
            wave.radius += wave.speed;
            wave.opacity = 1 - (wave.radius / wave.maxRadius);
            
            if (wave.opacity > 0) {
                this.ctx.strokeStyle = `rgba(0, 255, 255, ${wave.opacity * 0.3})`;
                this.ctx.lineWidth = 2;
                this.ctx.beginPath();
                this.ctx.arc(wave.x, wave.y, wave.radius, 0, Math.PI * 2);
                this.ctx.stroke();
                
                // Inner wave
                this.ctx.strokeStyle = `rgba(0, 255, 0, ${wave.opacity * 0.5})`;
                this.ctx.lineWidth = 1;
                this.ctx.beginPath();
                this.ctx.arc(wave.x, wave.y, wave.radius * 0.7, 0, Math.PI * 2);
                this.ctx.stroke();
                
                return true;
            }
            return false;
        });
        
        // Update particles
        this.particles.forEach((p, i) => {
            // Quantum oscillation
            p.phase += 0.02 * p.frequency;
            const quantum = Math.sin(p.phase + this.time) * p.amplitude;
            const wave = Math.sin(this.waveOffset + p.x * 0.01) * 5;
            
            // Update position with quantum mechanics
            p.x += p.vx + Math.sin(p.phase) * 0.5;
            p.y += p.vy + Math.cos(p.phase) * 0.5 + wave;
            p.z += p.vz;
            
            // 3D rotation effect
            const centerX = this.canvas.width / 2;
            const centerY = this.canvas.height / 2;
            const dx = p.x - centerX;
            const dy = p.y - centerY;
            const angle = this.time * 0.1;
            
            p.x = centerX + dx * Math.cos(angle) - dy * Math.sin(angle) * 0.1;
            p.y = centerY + dx * Math.sin(angle) * 0.1 + dy * Math.cos(angle);
            
            // Boundaries with quantum tunneling
            if (p.x < -50) p.x = this.canvas.width + 50;
            if (p.x > this.canvas.width + 50) p.x = -50;
            if (p.y < -50) p.y = this.canvas.height + 50;
            if (p.y > this.canvas.height + 50) p.y = -50;
            if (p.z < 0 || p.z > 1000) p.vz *= -1;
            
            // Mouse interaction with stronger force
            const mdx = p.x - this.mouse.x;
            const mdy = p.y - this.mouse.y;
            const dist = Math.sqrt(mdx * mdx + mdy * mdy);
            if (dist < 150) {
                const force = (1 - dist / 150) * 0.5;
                p.vx += mdx * force * 0.01;
                p.vy += mdy * force * 0.01;
                p.brightness = Math.min(1, p.brightness + 0.1);
            } else {
                p.brightness = Math.max(0.5, p.brightness - 0.01);
            }
        });
        
        // Draw quantum field lines
        this.ctx.strokeStyle = 'rgba(0, 255, 0, 0.03)';
        this.ctx.lineWidth = 1;
        
        for (let i = 0; i < this.particles.length; i += 5) {
            for (let j = i + 1; j < this.particles.length; j += 5) {
                const p1 = this.particles[i];
                const p2 = this.particles[j];
                const dist = Math.sqrt(
                    Math.pow(p1.x - p2.x, 2) + 
                    Math.pow(p1.y - p2.y, 2)
                );
                
                if (dist < 100) {
                    this.ctx.beginPath();
                    this.ctx.moveTo(p1.x, p1.y);
                    
                    // Curved quantum connection
                    const cx = (p1.x + p2.x) / 2 + Math.sin(this.time + i) * 20;
                    const cy = (p1.y + p2.y) / 2 + Math.cos(this.time + j) * 20;
                    this.ctx.quadraticCurveTo(cx, cy, p2.x, p2.y);
                    
                    this.ctx.globalAlpha = (1 - dist / 100) * 0.3;
                    this.ctx.stroke();
                }
            }
        }
        
        // Draw bright entanglement beams
        this.entanglementPairs.forEach(([i, j]) => {
            const p1 = this.particles[i];
            const p2 = this.particles[j];
            
            if (p1 && p2) {
                const gradient = this.ctx.createLinearGradient(p1.x, p1.y, p2.x, p2.y);
                gradient.addColorStop(0, 'rgba(0, 255, 255, 0.8)');
                gradient.addColorStop(0.5, 'rgba(255, 255, 0, 0.6)');
                gradient.addColorStop(1, 'rgba(255, 0, 255, 0.8)');
                
                this.ctx.strokeStyle = gradient;
                this.ctx.lineWidth = 2;
                this.ctx.globalAlpha = 0.6 + Math.sin(this.time * 3) * 0.3;
                
                this.ctx.beginPath();
                this.ctx.moveTo(p1.x, p1.y);
                
                // Lightning effect
                const segments = 5;
                for (let k = 1; k <= segments; k++) {
                    const t = k / segments;
                    const x = p1.x + (p2.x - p1.x) * t + (Math.random() - 0.5) * 20;
                    const y = p1.y + (p2.y - p1.y) * t + (Math.random() - 0.5) * 20;
                    this.ctx.lineTo(x, y);
                }
                
                this.ctx.stroke();
            }
        });
        
        // Draw particles with glow
        this.particles.forEach(p => {
            const scale = (1000 - p.z) / 1000;
            const size = p.size * scale * (p.type === 'quantum' ? 1.5 : 1);
            
            this.ctx.globalAlpha = scale * p.brightness;
            
            // Outer glow
            const glowGradient = this.ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, size * 4);
            glowGradient.addColorStop(0, p.color + '0.8)');
            glowGradient.addColorStop(0.5, p.color + '0.3)');
            glowGradient.addColorStop(1, p.color + '0)');
            
            this.ctx.fillStyle = glowGradient;
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, size * 4, 0, Math.PI * 2);
            this.ctx.fill();
            
            // Core particle
            this.ctx.fillStyle = p.color + '1)';
            this.ctx.shadowBlur = 20;
            this.ctx.shadowColor = p.color + '1)';
            
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, size, 0, Math.PI * 2);
            this.ctx.fill();
            
            // Quantum particles get extra effects
            if (p.type === 'quantum') {
                this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
                this.ctx.lineWidth = 1;
                this.ctx.beginPath();
                this.ctx.arc(p.x, p.y, size * 2, 0, Math.PI * 2);
                this.ctx.stroke();
            }
        });
        
        // Update and draw quantum bursts
        this.quantumBursts = this.quantumBursts.filter(burst => {
            burst.radius += 8;
            burst.opacity = 1 - (burst.radius / burst.maxRadius);
            
            if (burst.opacity > 0) {
                this.ctx.strokeStyle = burst.color;
                this.ctx.lineWidth = 3;
                this.ctx.globalAlpha = burst.opacity;
                
                this.ctx.beginPath();
                this.ctx.arc(burst.x, burst.y, burst.radius, 0, Math.PI * 2);
                this.ctx.stroke();
                
                // Inner rings
                this.ctx.lineWidth = 1;
                this.ctx.globalAlpha = burst.opacity * 0.5;
                this.ctx.beginPath();
                this.ctx.arc(burst.x, burst.y, burst.radius * 0.6, 0, Math.PI * 2);
                this.ctx.stroke();
                
                return true;
            }
            return false;
        });
        
        this.ctx.globalAlpha = 1;
        this.ctx.shadowBlur = 0;
    }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => new QuantumField());
} else {
    new QuantumField();
}