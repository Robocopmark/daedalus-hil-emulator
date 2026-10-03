import pygame
import serial
import time

COM_PORT = 'COM3' 
BAUD_RATE = 921600

# Captured Binary Payloads
ENABLE_HEX = "41541807ebfc0800000000000000000d0a"
STOP_HEX   = "41542007ebfc0800000000000000000d0a"

def build_dynamic_jog_packet(stick_input):
    """
    Translates PS5 analog stick input (-1.0 to 1.0) into the proprietary 
    16-bit binary payload expected by the Seeed Studio adapter.
    """
    base_center = 0x7FFF # Zero movement
    
    # Scale the stick input to a max offset of ~1000 (adjust for faster/slower speed)
    offset = int(stick_input * 1000.0)
    target_val = base_center + offset
    
    # Clamp to safe 16-bit limits to prevent integer overflow faults
    target_val = max(0x0000, min(0xFFFF, target_val))
    
    # Format as 4-character uppercase hex (e.g., '828E')
    hex_val = f"{target_val:04X}"
    
    # Construct the full packet: Header + ID + DLC + Static Payload + Dynamic Value + \r\n
    packet_hex = f"41549007ebfc08057000000701{hex_val}0d0a"
    return bytes.fromhex(packet_hex)

def run_console():
    print("Initializing Surgeon Console Engine...")
    pygame.init()
    pygame.joystick.init()
    
    if pygame.joystick.get_count() == 0:
        print("Fatal: DualSense Controller not detected.")
        return

    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    
    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
        print(f"HIL Bridge Active. Waking motor using captured binary sequence...")
        
        # Send Ground-Truth Enable Packet
        ser.write(bytes.fromhex(ENABLE_HEX))
        time.sleep(0.1)

        print("Teleoperation loop active. Move the left analog stick.")

        while True:
            pygame.event.pump()
            stick_input = joystick.get_axis(0) 
            
            # Deadzone filtering
            if abs(stick_input) < 0.05:
                stick_input = 0.0

            # Generate and transmit the dynamic binary hex packet
            raw_packet = build_dynamic_jog_packet(stick_input)
            ser.write(raw_packet)
            
            # Read telemetry from adapter to clear buffer
            if ser.in_waiting > 0:
                ser.read(ser.in_waiting)
                
            time.sleep(0.02) # 50Hz control loop

    except serial.SerialException as e:
        print(f"Hardware error: {e}")
    except KeyboardInterrupt:
        print("\nHalting teleoperation...")
        ser.write(bytes.fromhex(STOP_HEX))
        ser.close()
        print("Motor safely disabled.")

if __name__ == '__main__':
    run_console()