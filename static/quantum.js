// Quantum Terminal Background Animation
class QuantumField {
    constructor() {
        this.canvas = document.getElementById('quantum-canvas');
        this.ctx = this.canvas.getContext('2d');
        this.particles = [];
        this.particleCount = 150;
        this.connections = [];
        this.mouse = { x: 0, y: 0 };
        
        this.init();
        this.animate();
        
        // Mouse interaction
        window.addEventListener('mousemove', (e) => {
            this.mouse.x = e.clientX;
            this.mouse.y = e.clientY;
        });
        
        window.addEventListener('resize', () => this.resize());
    }
    
    init() {
        this.resize();
        
        // Create particles
        for (let i = 0; i < this.particleCount; i++) {
            this.particles.push({
                x: Math.random() * this.canvas.width,
                y: Math.random() * this.canvas.height,
                z: Math.random() * 1000,
                vx: (Math.random() - 0.5) * 0.5,
                vy: (Math.random() - 0.5) * 0.5,
                vz: (Math.random() - 0.5) * 0.5,
                phase: Math.random() * Math.PI * 2,
                entangled: null,
                color: `hsl(${120 + Math.random() * 60}, 100%, 50%)`
            });
        }
        
        // Create entanglements
        for (let i = 0; i < this.particleCount; i++) {
            if (Math.random() < 0.3) {
                const j = Math.floor(Math.random() * this.particleCount);
                if (i !== j) {
                    this.particles[i].entangled = j;
                    this.particles[j].entangled = i;
                }
            }
        }
    }
    
    resize() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
    }
    
    animate() {
        requestAnimationFrame(() => this.animate());
        
        // Clear with fade effect
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.05)';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        const time = Date.now() * 0.001;
        
        // Update particles
        this.particles.forEach((p, i) => {
            // Quantum oscillation
            p.phase += 0.02;
            const quantum = Math.sin(p.phase + time) * 0.5;
            
            // Update position
            p.x += p.vx + quantum;
            p.y += p.vy + Math.cos(p.phase) * 0.2;
            p.z += p.vz;
            
            // Boundaries
            if (p.x < 0 || p.x > this.canvas.width) p.vx *= -1;
            if (p.y < 0 || p.y > this.canvas.height) p.vy *= -1;
            if (p.z < 0 || p.z > 1000) p.vz *= -1;
            
            // Mouse interaction
            const dx = p.x - this.mouse.x;
            const dy = p.y - this.mouse.y;
            const dist = Math.sqrt(dx * dx + dy * dy);
            if (dist < 100) {
                p.vx += dx * 0.0001;
                p.vy += dy * 0.0001;
            }
        });
        
        // Draw connections
        this.ctx.strokeStyle = 'rgba(0, 255, 0, 0.1)';
        this.ctx.lineWidth = 1;
        
        this.particles.forEach((p1, i) => {
            this.particles.forEach((p2, j) => {
                if (i < j) {
                    const dist = Math.sqrt(
                        Math.pow(p1.x - p2.x, 2) + 
                        Math.pow(p1.y - p2.y, 2) + 
                        Math.pow(p1.z - p2.z, 2)
                    );
                    
                    if (dist < 150) {
                        this.ctx.beginPath();
                        this.ctx.moveTo(p1.x, p1.y);
                        this.ctx.lineTo(p2.x, p2.y);
                        this.ctx.globalAlpha = 1 - dist / 150;
                        this.ctx.stroke();
                    }
                }
            });
            
            // Draw entanglement
            if (p1.entangled !== null) {
                const p2 = this.particles[p1.entangled];
                this.ctx.strokeStyle = 'rgba(0, 255, 255, 0.3)';
                this.ctx.beginPath();
                this.ctx.moveTo(p1.x, p1.y);
                this.ctx.lineTo(p2.x, p2.y);
                this.ctx.stroke();
            }
        });
        
        // Draw particles
        this.particles.forEach(p => {
            const scale = (1000 - p.z) / 1000;
            const size = 3 * scale;
            
            this.ctx.globalAlpha = scale;
            this.ctx.fillStyle = p.color;
            this.ctx.shadowBlur = 10;
            this.ctx.shadowColor = p.color;
            
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, size, 0, Math.PI * 2);
            this.ctx.fill();
            
            // Quantum glow
            this.ctx.globalAlpha = scale * 0.3;
            this.ctx.beginPath();
            this.ctx.arc(p.x, p.y, size * 3, 0, Math.PI * 2);
            this.ctx.fill();
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