import serial
import threading
import sys
import time

# ESP32-C6 COM port
COM_PORT = "COM3" 
BAUD_RATE = 115200

try:
    # Connect to the ESP32-C6
    analyzer = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    print(f"Successfully connected to {COM_PORT}")
except serial.SerialException:
    print(f"Error: Could not open {COM_PORT}. Check your connection.")
    sys.exit(1)

# --- THE RECEIVER THREAD ---
# This function runs continuously in the background, grabbing the decoded 
# text strings sent by your Zephyr usb_streamer.c file.
def listen_to_hardware():
    while True:
        if analyzer.in_waiting > 0:
            try:
                # Read the incoming line and decode the ASCII bytes
                raw_line = analyzer.readline().decode('utf-8').strip()
                if raw_line:
                    print(f"\n{raw_line}")
            except Exception as e:
                pass

# Start the background listener
listener = threading.Thread(target=listen_to_hardware, daemon=True)
listener.start()

# --- THE COMMAND UI ---
time.sleep(1) # Give the USB port a second to settle
print("\n Protocol Analyzer Control")
print("Press 'S' to Sniff SPI (Pins 2,3,4,5)")
print("Press 'I' to Sniff I2C (Pins 0,1)")
print("Press 'D' to Sniff Dual Mode (SPI + I2C)")
print("Press 'Q' to Quit")
print("=====================================\n")

while True:
    # Wait for the user to type a command
    user_input = input("Enter Command: ").strip().upper()
    
    if user_input in ['S', 'I', 'D']:
        # Send the exact byte the Zephyr ISR is waiting for
        analyzer.write(user_input.encode('utf-8'))
        print(f"-> Command '{user_input}' sent to hardware matrix.")
    
    elif user_input == 'Q':
        print("Closing analyzer connection...")
        analyzer.close()
        sys.exit(0)
    
    else:
        print("Invalid command.")