package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"

	"github.com/creack/pty"
	"github.com/gorilla/websocket"
	"golang.org/x/crypto/ssh"
)

// Message types for WebSocket communication
type MessageType string

const (
	MessageTypeInput     MessageType = "input"
	MessageTypeOutput    MessageType = "output"
	MessageTypeResize    MessageType = "resize"
	MessageTypeConnect   MessageType = "connect"
	MessageTypeAnimation MessageType = "animation"
)

// Message structure for WebSocket
type Message struct {
	Type      MessageType     `json:"type"`
	Data      string          `json:"data,omitempty"`
	Cols      int             `json:"cols,omitempty"`
	Rows      int             `json:"rows,omitempty"`
	Animation AnimationType   `json:"animation,omitempty"`
	X         int             `json:"x,omitempty"`
	Y         int             `json:"y,omitempty"`
	Target    string          `json:"target,omitempty"` // "local" or "remote"
}

// Animation types matching Flutter frontend
type AnimationType string

const (
	AnimationMatrixRain      AnimationType = "matrix_rain"
	AnimationWormholePortal  AnimationType = "wormhole_portal"
	AnimationQuantumExplosion AnimationType = "quantum_explosion"
	AnimationDNAHelix        AnimationType = "dna_helix"
	AnimationGlitchText      AnimationType = "glitch_text"
	AnimationNeuralNetwork   AnimationType = "neural_network"
	AnimationCosmicRays      AnimationType = "cosmic_rays"
	AnimationParticleFountain AnimationType = "particle_fountain"
	AnimationTimeWarp        AnimationType = "time_warp"
	AnimationQuantumTunnel   AnimationType = "quantum_tunnel"
)

// Terminal session
type TerminalSession struct {
	ptmx       *os.File
	sshClient  *ssh.Client
	sshSession *ssh.Session
	isRemote   bool
}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // Allow all origins for development
	},
}

// Command detection for animations
var commandAnimations = map[string]AnimationType{
	"ls":       AnimationMatrixRain,
	"cd":       AnimationWormholePortal,
	"rm -rf":   AnimationQuantumExplosion,
	"git":      AnimationDNAHelix,
	"sudo":     AnimationGlitchText,
	"python":   AnimationNeuralNetwork,
	"vim":      AnimationCosmicRays,
	"make":     AnimationParticleFountain,
	"history":  AnimationTimeWarp,
	"ssh":      AnimationQuantumTunnel,
}

func handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket upgrade failed: %v", err)
		return
	}
	defer conn.Close()

	session := &TerminalSession{}
	commandBuffer := ""

	// Start local terminal by default
	if err := startLocalTerminal(session); err != nil {
		log.Printf("Failed to start local terminal: %v", err)
		return
	}
	defer session.Close()

	// Handle terminal output
	go func() {
		buf := make([]byte, 1024)
		for {
			n, err := session.Read(buf)
			if err != nil {
				log.Printf("Terminal read error: %v", err)
				return
			}

			output := string(buf[:n])
			
			// Send output to frontend
			msg := Message{
				Type: MessageTypeOutput,
				Data: output,
			}
			if err := conn.WriteJSON(msg); err != nil {
				log.Printf("WebSocket write error: %v", err)
				return
			}

			// Update command buffer for animation detection
			for _, char := range output {
				if char == '\n' || char == '\r' {
					// Check for animation trigger
					if animation := detectAnimation(commandBuffer); animation != "" {
						animMsg := Message{
							Type:      MessageTypeAnimation,
							Animation: animation,
							X:         10, // These would be calculated based on cursor position
							Y:         10,
						}
						conn.WriteJSON(animMsg)
					}
					commandBuffer = ""
				} else if char == '\b' || char == 127 { // Backspace
					if len(commandBuffer) > 0 {
						commandBuffer = commandBuffer[:len(commandBuffer)-1]
					}
				} else if char >= 32 && char < 127 { // Printable ASCII
					commandBuffer += string(char)
				}
			}
		}
	}()

	// Handle WebSocket messages
	for {
		var msg Message
		if err := conn.ReadJSON(&msg); err != nil {
			log.Printf("WebSocket read error: %v", err)
			return
		}

		switch msg.Type {
		case MessageTypeInput:
			// Write input to terminal
			if _, err := session.Write([]byte(msg.Data)); err != nil {
				log.Printf("Terminal write error: %v", err)
			}

		case MessageTypeResize:
			// Resize terminal
			if err := session.Resize(msg.Cols, msg.Rows); err != nil {
				log.Printf("Terminal resize error: %v", err)
			}

		case MessageTypeConnect:
			// Connect to remote droplet
			if msg.Target == "remote" {
				session.Close()
				if err := startRemoteTerminal(session); err != nil {
					log.Printf("Failed to connect to droplet: %v", err)
					// Fall back to local terminal
					startLocalTerminal(session)
				}
			} else {
				// Switch back to local
				session.Close()
				startLocalTerminal(session)
			}
		}
	}
}

func startLocalTerminal(session *TerminalSession) error {
	// Start bash
	cmd := exec.Command("/bin/bash")
	
	// Start PTY
	ptmx, err := pty.Start(cmd)
	if err != nil {
		return fmt.Errorf("failed to start PTY: %w", err)
	}

	session.ptmx = ptmx
	session.isRemote = false
	return nil
}

func startRemoteTerminal(session *TerminalSession) error {
	// Load config
	config := loadConfig()
	
	// Setup SSH client config
	key, err := os.ReadFile(os.ExpandEnv(config.Droplet.KeyPath))
	if err != nil {
		return fmt.Errorf("failed to read SSH key: %w", err)
	}

	signer, err := ssh.ParsePrivateKey(key)
	if err != nil {
		return fmt.Errorf("failed to parse SSH key: %w", err)
	}

	sshConfig := &ssh.ClientConfig{
		User: config.Droplet.Username,
		Auth: []ssh.AuthMethod{
			ssh.PublicKeys(signer),
		},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(), // TODO: Implement proper host key checking
	}

	// Connect to droplet
	addr := fmt.Sprintf("%s:%d", config.Droplet.Host, config.Droplet.Port)
	client, err := ssh.Dial("tcp", addr, sshConfig)
	if err != nil {
		return fmt.Errorf("failed to connect to droplet: %w", err)
	}

	// Start session
	sshSession, err := client.NewSession()
	if err != nil {
		client.Close()
		return fmt.Errorf("failed to create SSH session: %w", err)
	}

	// Request PTY
	modes := ssh.TerminalModes{
		ssh.ECHO:          1,
		ssh.TTY_OP_ISPEED: 14400,
		ssh.TTY_OP_OSPEED: 14400,
	}

	if err := sshSession.RequestPty("xterm-256color", 24, 80, modes); err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to request PTY: %w", err)
	}

	// Start shell
	if err := sshSession.Shell(); err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to start shell: %w", err)
	}

	session.sshClient = client
	session.sshSession = sshSession
	session.isRemote = true
	return nil
}

func (s *TerminalSession) Read(p []byte) (n int, err error) {
	if s.isRemote {
		// Read from SSH session stdout
		if s.sshSession.Stdout == nil {
			return 0, fmt.Errorf("SSH stdout not available")
		}
		// This would need proper implementation with pipes
	} else {
		return s.ptmx.Read(p)
	}
	return 0, nil
}

func (s *TerminalSession) Write(p []byte) (n int, err error) {
	if s.isRemote {
		// Write to SSH session stdin
		if s.sshSession.Stdin == nil {
			return 0, fmt.Errorf("SSH stdin not available")
		}
		// This would need proper implementation with pipes
	} else {
		return s.ptmx.Write(p)
	}
	return 0, nil
}

func (s *TerminalSession) Resize(cols, rows int) error {
	if s.isRemote {
		return s.sshSession.WindowChange(rows, cols)
	} else {
		ws := &pty.Winsize{Cols: uint16(cols), Rows: uint16(rows)}
		return pty.Setsize(s.ptmx, ws)
	}
}

func (s *TerminalSession) Close() {
	if s.isRemote {
		if s.sshSession != nil {
			s.sshSession.Close()
		}
		if s.sshClient != nil {
			s.sshClient.Close()
		}
	} else {
		if s.ptmx != nil {
			s.ptmx.Close()
		}
	}
}

func detectAnimation(command string) AnimationType {
	for cmd, anim := range commandAnimations {
		if len(command) >= len(cmd) && command[:len(cmd)] == cmd {
			return anim
		}
	}
	return ""
}

// Config structure
type Config struct {
	Droplet struct {
		Host     string `json:"host"`
		Port     int    `json:"port"`
		Username string `json:"username"`
		KeyPath  string `json:"keyPath"`
	} `json:"droplet"`
}

func loadConfig() Config {
	var config Config
	
	// Default values
	config.Droplet.Port = 22
	config.Droplet.Username = "root"
	config.Droplet.KeyPath = "~/.ssh/id_rsa"
	
	// Try to load from file
	data, err := os.ReadFile("config.json")
	if err == nil {
		json.Unmarshal(data, &config)
	}
	
	return config
}

func main() {
	// Serve WebSocket endpoint
	http.HandleFunc("/ws", handleWebSocket)
	
	// Health check endpoint
	http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("OK"))
	})

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	log.Printf("Quantum Terminal Backend starting on port %s", port)
	if err := http.ListenAndServe(":"+port, nil); err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}