package main

import (
	"io"
	"os"
	"os/exec"

	"github.com/creack/pty"
	"golang.org/x/crypto/ssh"
)

// Fixed terminal session with proper pipe handling
type FixedTerminalSession struct {
	ptmx         *os.File
	cmd          *exec.Cmd
	sshClient    *ssh.Client
	sshSession   *ssh.Session
	sshStdin     io.WriteCloser
	sshStdout    io.Reader
	isRemote     bool
}

func (s *FixedTerminalSession) Read(p []byte) (n int, err error) {
	if s.isRemote && s.sshStdout != nil {
		return s.sshStdout.Read(p)
	} else if s.ptmx != nil {
		return s.ptmx.Read(p)
	}
	return 0, io.EOF
}

func (s *FixedTerminalSession) Write(p []byte) (n int, err error) {
	if s.isRemote && s.sshStdin != nil {
		return s.sshStdin.Write(p)
	} else if s.ptmx != nil {
		return s.ptmx.Write(p)
	}
	return 0, io.EOF
}

func (s *FixedTerminalSession) Resize(cols, rows int) error {
	if s.isRemote && s.sshSession != nil {
		return s.sshSession.WindowChange(rows, cols)
	} else if s.ptmx != nil {
		ws := &pty.Winsize{Cols: uint16(cols), Rows: uint16(rows)}
		return pty.Setsize(s.ptmx, ws)
	}
	return nil
}

func (s *FixedTerminalSession) Close() {
	if s.isRemote {
		if s.sshStdin != nil {
			s.sshStdin.Close()
		}
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
		if s.cmd != nil && s.cmd.Process != nil {
			s.cmd.Process.Kill()
		}
	}
}