#!/usr/bin/env python3
"""
SUNLU AMS Heater Auto-Vent Serial Monitor
==========================================
A Windows serial monitor application with auto-detection, auto-reconnect,
and color-coded output parsing for the SUNLU AMS Heater controller.

Features:
- Auto-detect Arduino/SAMD21 boards
- Auto-reconnect on disconnect
- Color-coded output for easy reading
- Clean, simple UI

Requirements:
- Python 3.7+
- pyserial
- tkinter (usually included with Python)

Usage:
    python serial_monitor.py

Or build as standalone .exe:
    pip install pyinstaller
    pyinstaller --onefile --windowed --name="SUNLU_SerialMonitor" serial_monitor.py

Author: Generated with Claude Code
License: MIT
"""

import tkinter as tk
from tkinter import scrolledtext, ttk
import serial
import serial.tools.list_ports
import threading
import time
import sys

class SerialMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("SUNLU AMS Heater - Serial Monitor")
        self.root.geometry("900x600")

        # Serial connection
        self.serial_port = None
        self.is_connected = False
        self.auto_reconnect = True
        self.baud_rate = 115200
        self.read_thread = None
        self.stop_thread = False

        # Setup UI
        self.setup_ui()

        # Start auto-detection
        self.root.after(1000, self.auto_detect_and_connect)

    def setup_ui(self):
        """Create the UI components"""
        # Top frame - controls
        control_frame = ttk.Frame(self.root, padding="10")
        control_frame.pack(fill=tk.X)

        # Port selection
        ttk.Label(control_frame, text="Port:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(control_frame, textvariable=self.port_var,
                                       width=15, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=5)

        # Refresh button
        ttk.Button(control_frame, text="Refresh Ports",
                  command=self.refresh_ports).pack(side=tk.LEFT, padx=5)

        # Connect/Disconnect button
        self.connect_btn = ttk.Button(control_frame, text="Connect",
                                      command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)

        # Clear button
        ttk.Button(control_frame, text="Clear",
                  command=self.clear_output).pack(side=tk.LEFT, padx=5)

        # Auto-reconnect checkbox
        self.auto_reconnect_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(control_frame, text="Auto-reconnect",
                       variable=self.auto_reconnect_var,
                       command=self.toggle_auto_reconnect).pack(side=tk.LEFT, padx=20)

        # Status label
        self.status_label = ttk.Label(control_frame, text="Disconnected",
                                      foreground="red")
        self.status_label.pack(side=tk.RIGHT, padx=5)

        # Text output area
        self.text_area = scrolledtext.ScrolledText(
            self.root,
            wrap=tk.WORD,
            bg="#1e1e1e",
            fg="#d4d4d4",
            font=("Consolas", 10),
            insertbackground="white"
        )
        self.text_area.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Configure color tags
        self.text_area.tag_config("green", foreground="#4ec9b0")
        self.text_area.tag_config("blue", foreground="#569cd6")
        self.text_area.tag_config("yellow", foreground="#dcdcaa")
        self.text_area.tag_config("red", foreground="#f48771")
        self.text_area.tag_config("cyan", foreground="#4fc1ff")
        self.text_area.tag_config("white", foreground="#d4d4d4")
        self.text_area.tag_config("gray", foreground="#808080")

        # Status bar
        status_frame = ttk.Frame(self.root)
        status_frame.pack(fill=tk.X, side=tk.BOTTOM)
        self.status_bar = ttk.Label(status_frame, text="Ready", relief=tk.SUNKEN)
        self.status_bar.pack(fill=tk.X, padx=5, pady=2)

        # Initial port refresh
        self.refresh_ports()

    def refresh_ports(self):
        """Refresh the list of available COM ports"""
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]

        self.port_combo['values'] = port_list

        # Try to auto-select Arduino/SAMD21
        arduino_port = None
        for port in ports:
            # Look for common Arduino/Seeed VID/PID
            if port.vid in [0x2341, 0x2886]:  # Arduino, Seeed
                arduino_port = port.device
                break

        if arduino_port:
            self.port_var.set(arduino_port)
            self.status_bar.config(text=f"Found Arduino board on {arduino_port}")
        elif port_list:
            self.port_var.set(port_list[0])

    def toggle_auto_reconnect(self):
        """Toggle auto-reconnect feature"""
        self.auto_reconnect = self.auto_reconnect_var.get()

    def toggle_connection(self):
        """Connect or disconnect from serial port"""
        if self.is_connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        """Connect to the selected serial port"""
        port = self.port_var.get()
        if not port:
            self.append_text("No port selected!\n", "red")
            return

        try:
            self.serial_port = serial.Serial(port, self.baud_rate, timeout=0.1)
            self.is_connected = True
            self.status_label.config(text="Connected", foreground="green")
            self.connect_btn.config(text="Disconnect")
            self.status_bar.config(text=f"Connected to {port} at {self.baud_rate} baud")

            # Start reading thread
            self.stop_thread = False
            self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
            self.read_thread.start()

            self.append_text(f"=== Connected to {port} ===\n", "cyan")

        except Exception as e:
            self.append_text(f"Connection error: {str(e)}\n", "red")
            self.status_bar.config(text=f"Connection failed: {str(e)}")

    def disconnect(self):
        """Disconnect from serial port"""
        self.stop_thread = True
        if self.read_thread:
            self.read_thread.join(timeout=1)

        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

        self.is_connected = False
        self.status_label.config(text="Disconnected", foreground="red")
        self.connect_btn.config(text="Connect")
        self.status_bar.config(text="Disconnected")
        self.append_text("=== Disconnected ===\n", "gray")

    def read_serial(self):
        """Read from serial port in background thread"""
        while not self.stop_thread:
            try:
                if self.serial_port and self.serial_port.is_open:
                    if self.serial_port.in_waiting:
                        line = self.serial_port.readline().decode('utf-8', errors='ignore')
                        if line:
                            self.parse_and_append(line)
                else:
                    break
            except Exception as e:
                self.append_text(f"Read error: {str(e)}\n", "red")
                break

            time.sleep(0.01)

        # If disconnected and auto-reconnect is enabled, try to reconnect
        if not self.stop_thread and self.auto_reconnect:
            self.root.after(2000, self.auto_reconnect_attempt)

    def auto_reconnect_attempt(self):
        """Attempt to reconnect if auto-reconnect is enabled"""
        if not self.is_connected and self.auto_reconnect:
            port = self.port_var.get()
            self.status_bar.config(text=f"Attempting to reconnect to {port}...")
            self.connect()

    def auto_detect_and_connect(self):
        """Auto-detect and connect on startup"""
        if not self.is_connected:
            self.refresh_ports()
            port = self.port_var.get()
            if port:
                self.connect()

    def parse_and_append(self, line):
        """Parse line and append with appropriate color"""
        # Determine color based on content
        color = "white"  # default

        if ">>>" in line or "OPENING" in line or "Opening" in line:
            color = "green"
        elif "CLOSING" in line or "Closing" in line or "DELAY EXPIRED" in line:
            color = "blue"
        elif "DEBUG:" in line or "debug" in line.lower():
            color = "yellow"
        elif "ERROR" in line or "error" in line.lower() or "FAIL" in line:
            color = "red"
        elif "====" in line or "CALIBRATION" in line or "Learning" in line or "LEARNING" in line:
            color = "cyan"
        elif "WARNING" in line or "warning" in line.lower():
            color = "yellow"

        self.append_text(line, color)

    def append_text(self, text, color="white"):
        """Append text to the output area with color"""
        def _append():
            self.text_area.config(state=tk.NORMAL)
            self.text_area.insert(tk.END, text, color)
            self.text_area.see(tk.END)
            self.text_area.config(state=tk.DISABLED)

        # Thread-safe update
        self.root.after(0, _append)

    def clear_output(self):
        """Clear the output text area"""
        self.text_area.config(state=tk.NORMAL)
        self.text_area.delete(1.0, tk.END)
        self.text_area.config(state=tk.DISABLED)

    def on_closing(self):
        """Handle window close event"""
        self.disconnect()
        self.root.destroy()

def main():
    root = tk.Tk()
    app = SerialMonitor(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()

if __name__ == "__main__":
    main()
