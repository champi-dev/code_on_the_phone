package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"time"

	"github.com/creack/pty"
	"github.com/gorilla/websocket"
	"golang.org/x/crypto/ssh"
	"golang.org/x/crypto/ssh/knownhosts"
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
	sshStdout  io.Reader
	sshStdin   io.WriteCloser
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

	// Set up ping/pong handlers
	conn.SetReadDeadline(time.Now().Add(60 * time.Second))
	conn.SetPongHandler(func(string) error {
		conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		return nil
	})

	// Start ping ticker
	pingTicker := time.NewTicker(30 * time.Second)
	defer pingTicker.Stop()

	// Start local terminal by default
	if err := startLocalTerminal(session); err != nil {
		log.Printf("Failed to start local terminal: %v", err)
		// Send error to client
		conn.WriteJSON(Message{
			Type: MessageTypeOutput,
			Data: fmt.Sprintf("\r\n\x1b[31mFailed to start terminal: %v\x1b[0m\r\n", err),
		})
		return
	}
	defer session.Close()

	// Handle terminal output with streaming for large outputs
	go func() {
		const chunkSize = 4096
		buf := make([]byte, chunkSize)
		outputBuffer := make([]byte, 0, chunkSize*2)
		flushTicker := time.NewTicker(50 * time.Millisecond) // Flush buffer regularly
		defer flushTicker.Stop()

		// Function to send buffered output
		sendOutput := func(force bool) {
			if len(outputBuffer) > 0 && (force || len(outputBuffer) >= chunkSize) {
				output := string(outputBuffer)
				
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
								X:         10,
								Y:         10,
							}
							conn.WriteJSON(animMsg)
						}
						commandBuffer = ""
					} else if char == '\b' || char == 127 {
						if len(commandBuffer) > 0 {
							commandBuffer = commandBuffer[:len(commandBuffer)-1]
						}
					} else if char >= 32 && char < 127 {
						commandBuffer += string(char)
					}
				}
				
				outputBuffer = outputBuffer[:0] // Clear buffer
			}
		}

		for {
			select {
			case <-flushTicker.C:
				// Periodic flush for interactive output
				sendOutput(true)
				
			default:
				n, err := session.Read(buf)
				if err != nil {
					if err == io.EOF {
						log.Printf("Terminal EOF reached")
					} else {
						log.Printf("Terminal read error: %v", err)
					}
					// Flush any remaining output
					sendOutput(true)
					// Send error to client
					errMsg := Message{
						Type: MessageTypeOutput,
						Data: fmt.Sprintf("\r\n\x1b[31mTerminal read error: %v\x1b[0m\r\n", err),
					}
					conn.WriteJSON(errMsg)
					return
				}

				if n > 0 {
					outputBuffer = append(outputBuffer, buf[:n]...)
					// Send if buffer is getting large
					if len(outputBuffer) >= chunkSize {
						sendOutput(false)
					}
				}

			}
		}
	}()

	// Channel to handle ping ticker
	done := make(chan struct{})
	defer close(done)

	// Handle ping ticker in separate goroutine
	go func() {
		for {
			select {
			case <-pingTicker.C:
				if err := conn.WriteControl(websocket.PingMessage, []byte{}, time.Now().Add(10*time.Second)); err != nil {
					log.Printf("Ping error: %v", err)
					return
				}
			case <-done:
				return
			}
		}
	}()

	// Handle WebSocket messages
	for {
		var msg Message
		if err := conn.ReadJSON(&msg); err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("WebSocket unexpected close error: %v", err)
			}
			return
		}

		switch msg.Type {
		case MessageTypeInput:
			// Write input to terminal
			if _, err := session.Write([]byte(msg.Data)); err != nil {
				log.Printf("Terminal write error: %v", err)
				// Send error back to client
				conn.WriteJSON(Message{
					Type: MessageTypeOutput,
					Data: fmt.Sprintf("\r\n\x1b[31mError: %v\x1b[0m\r\n", err),
				})
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
					// Send error to client
					conn.WriteJSON(Message{
						Type: MessageTypeOutput,
						Data: fmt.Sprintf("\r\n\x1b[31mFailed to connect: %v\x1b[0m\r\n", err),
					})
					// Fall back to local terminal
					if err := startLocalTerminal(session); err != nil {
						log.Printf("Failed to restart local terminal: %v", err)
					}
				}
			} else {
				// Switch back to local
				session.Close()
				if err := startLocalTerminal(session); err != nil {
					log.Printf("Failed to start local terminal: %v", err)
					conn.WriteJSON(Message{
						Type: MessageTypeOutput,
						Data: fmt.Sprintf("\r\n\x1b[31mFailed to start local terminal: %v\x1b[0m\r\n", err),
					})
				}
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
		HostKeyCallback: getHostKeyCallback(config),
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

	// Setup pipes for SSH I/O
	stdin, err := sshSession.StdinPipe()
	if err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to create stdin pipe: %w", err)
	}

	stdout, err := sshSession.StdoutPipe()
	if err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to create stdout pipe: %w", err)
	}

	stderr, err := sshSession.StderrPipe()
	if err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to create stderr pipe: %w", err)
	}

	// Combine stdout and stderr
	combinedOutput := io.MultiReader(stdout, stderr)

	// Start shell
	if err := sshSession.Shell(); err != nil {
		sshSession.Close()
		client.Close()
		return fmt.Errorf("failed to start shell: %w", err)
	}

	session.sshClient = client
	session.sshSession = sshSession
	session.sshStdin = stdin
	session.sshStdout = combinedOutput
	session.isRemote = true
	return nil
}

func (s *TerminalSession) Read(p []byte) (n int, err error) {
	if s.isRemote {
		if s.sshStdout == nil {
			return 0, fmt.Errorf("SSH stdout not available")
		}
		return s.sshStdout.Read(p)
	} else {
		return s.ptmx.Read(p)
	}
}

func (s *TerminalSession) Write(p []byte) (n int, err error) {
	if s.isRemote {
		if s.sshStdin == nil {
			return 0, fmt.Errorf("SSH stdin not available")
		}
		return s.sshStdin.Write(p)
	} else {
		return s.ptmx.Write(p)
	}
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

// getHostKeyCallback returns a host key callback function that verifies SSH host keys
func getHostKeyCallback(config Config) ssh.HostKeyCallback {
	knownHostsPath := filepath.Join(os.Getenv("HOME"), ".ssh", "quantum_terminal_known_hosts")
	
	// Create directory if it doesn't exist
	os.MkdirAll(filepath.Dir(knownHostsPath), 0700)
	
	// Check if known_hosts file exists
	if _, err := os.Stat(knownHostsPath); os.IsNotExist(err) {
		// Create empty file
		file, err := os.Create(knownHostsPath)
		if err != nil {
			log.Printf("Failed to create known_hosts file: %v", err)
			// Fall back to a callback that prompts for verification
			return func(hostname string, remote net.Addr, key ssh.PublicKey) error {
				return verifyHostKey(hostname, remote, key, knownHostsPath)
			}
		}
		file.Close()
	}
	
	// Try to use existing known_hosts
	hostKeyCallback, err := knownhosts.New(knownHostsPath)
	if err != nil {
		log.Printf("Failed to parse known_hosts: %v", err)
		// Fall back to verification callback
		return func(hostname string, remote net.Addr, key ssh.PublicKey) error {
			return verifyHostKey(hostname, remote, key, knownHostsPath)
		}
	}
	
	// Wrap the callback to handle unknown hosts
	return func(hostname string, remote net.Addr, key ssh.PublicKey) error {
		err := hostKeyCallback(hostname, remote, key)
		if err != nil {
			// Check if it's an unknown host error
			var keyErr *knownhosts.KeyError
			if errors.As(err, &keyErr) && len(keyErr.Want) == 0 {
				// Unknown host - verify and potentially add
				return verifyHostKey(hostname, remote, key, knownHostsPath)
			}
			// Other error (e.g., key mismatch)
			return err
		}
		return nil
	}
}

// verifyHostKey handles verification of unknown SSH host keys
func verifyHostKey(hostname string, remote net.Addr, key ssh.PublicKey, knownHostsPath string) error {
	// Format the host key entry
	hostKey := knownhosts.Line([]string{hostname}, key)
	fingerprint := ssh.FingerprintSHA256(key)
	
	log.Printf("SSH Host Key Verification:")
	log.Printf("Host: %s", hostname)
	log.Printf("Fingerprint: %s", fingerprint)
	log.Printf("Key Type: %s", key.Type())
	
	// In a production environment, you would prompt the user here
	// For now, we'll auto-accept but log a warning
	log.Printf("WARNING: Automatically accepting SSH host key for %s", hostname)
	log.Printf("In production, implement proper user verification!")
	
	// Add the host key to known_hosts
	file, err := os.OpenFile(knownHostsPath, os.O_APPEND|os.O_WRONLY, 0600)
	if err != nil {
		return fmt.Errorf("failed to open known_hosts file: %w", err)
	}
	defer file.Close()
	
	if _, err := file.WriteString(hostKey + "\n"); err != nil {
		return fmt.Errorf("failed to write host key: %w", err)
	}
	
	log.Printf("Added host key for %s to known_hosts", hostname)
	return nil
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