#!/usr/bin/env python3
"""
Smoke Test for Parallel Meshtastic Hardware
Verifies basic radio functionality between two devices
"""

import serial
import time
import sys
import argparse

def test_radio_ping(port1, port2, baudrate=115200, timeout=30):
    """Test basic radio ping between two devices"""
    print("=== Parallel Meshtastic Smoke Test ===")
    
    try:
        # Open serial connections
        ser1 = serial.Serial(port1, baudrate, timeout=1)
        ser2 = serial.Serial(port2, baudrate, timeout=1) if port2 else None
        
        print(f"Connected to {port1}" + (f" and {port2}" if port2 else ""))
        
        # Send ping from device 1
        print("Sending ping from device 1...")
        ser1.write(b"ping\n")
        
        # If we have two devices, listen on device 2
        if ser2:
            print("Listening for ping on device 2...")
            start_time = time.time()
            received = False
            
            while time.time() - start_time < timeout:
                if ser2.in_waiting:
                    data = ser2.read(ser2.in_waiting)
                    if b"PING" in data:
                        print("✓ Ping received successfully!")
                        received = True
                        break
                time.sleep(0.1)
            
            if not received:
                print("✗ Ping not received within timeout")
                return False
        
        # Test device status
        print("Checking device status...")
        ser1.write(b"status\n")
        time.sleep(1)
        
        response = ser1.read(ser1.in_waiting).decode('utf-8', errors='ignore')
        if "SX1262 Radio Status" in response:
            print("✓ Radio status OK")
        else:
            print("✗ Radio status check failed")
            return False
            
        print("✓ Smoke test passed!")
        return True
        
    except Exception as e:
        print(f"✗ Test failed: {e}")
        return False
    finally:
        if 'ser1' in locals():
            ser1.close()
        if 'ser2' in locals() and ser2:
            ser2.close()

def main():
    parser = argparse.ArgumentParser(description='Parallel Meshtastic Hardware Smoke Test')
    parser.add_argument('port1', help='First device serial port (e.g., COM3)')
    parser.add_argument('--port2', help='Second device serial port (optional)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--timeout', type=int, default=30, help='Test timeout in seconds (default: 30)')
    
    args = parser.parse_args()
    
    success = test_radio_ping(args.port1, args.port2, args.baudrate, args.timeout)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()