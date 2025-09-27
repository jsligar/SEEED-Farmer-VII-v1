#!/usr/bin/env python3
"""
Serial CLI for Parallel Meshtastic
Provides an interactive console for testing mesh commands
"""

import serial
import threading
import sys
import time

class SerialCLI:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.running = False
        
    def connect(self):
        """Connect to the device"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def read_thread(self):
        """Background thread to read serial data"""
        while self.running:
            if self.ser and self.ser.in_waiting:
                try:
                    data = self.ser.read(self.ser.in_waiting)
                    print(data.decode('utf-8', errors='ignore'), end='')
                except:
                    pass
            time.sleep(0.01)
    
    def run(self):
        """Main CLI loop"""
        if not self.connect():
            return
            
        self.running = True
        
        # Start read thread
        read_thread = threading.Thread(target=self.read_thread, daemon=True)
        read_thread.start()
        
        print("Parallel Meshtastic Serial CLI")
        print("Available commands: ping, send <message>, receive, status")
        print("Type 'exit' to quit")
        print("-" * 50)
        
        try:
            while True:
                user_input = input()
                
                if user_input.lower() in ['exit', 'quit']:
                    break
                    
                # Send command to device
                self.ser.write(f"{user_input}\n".encode())
                
        except KeyboardInterrupt:
            print("\nExiting...")
        finally:
            self.running = False
            if self.ser:
                self.ser.close()

def main():
    if len(sys.argv) != 2:
        print("Usage: python serial_cli.py <port>")
        print("Example: python serial_cli.py COM3")
        sys.exit(1)
    
    port = sys.argv[1]
    cli = SerialCLI(port)
    cli.run()

if __name__ == "__main__":
    main()