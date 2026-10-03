#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

int main() {
    int serial_port = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_port < 0) {
        std::cerr << "Fatal: Unable to open /dev/ttyUSB0." << std::endl;
        return 1;
    }

    struct termios tty;
    tcgetattr(serial_port, &tty);
    
    // --- THE CRITICAL FIX ---
    // Enforces absolute binary transparency. Disables OPOST (newline mutation), 
    // ECHO, and all other text-based terminal line disciplines.
    cfmakeraw(&tty); 
    
    cfsetispeed(&tty, B921600);
    cfsetospeed(&tty, B921600);
    
    // Non-blocking read setup
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; 

    tcsetattr(serial_port, TCSANOW, &tty);

    std::cout << "Jetson HIL Core Active. Absolute raw binary pipe established." << std::endl;

    // 1. The intercepted ENABLE binary payload
    uint8_t enable_cmd[] = {
        0x41, 0x54, 0x18, 0x07, 0xEB, 0xFC, 0x08, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x0A
    };
    
    if (write(serial_port, enable_cmd, sizeof(enable_cmd)) < 0) {
        std::cerr << "Error writing Enable command." << std::endl;
    }
    std::cout << "Sent Enable Packet..." << std::endl;
    usleep(500000); // 500ms delay to allow motor firmware to boot

    // 2. The intercepted JOG+ binary payload
    uint8_t jog_fwd[] = {
        0x41, 0x54, 0x90, 0x07, 0xEB, 0xFC, 0x08, 0x05, 
        0x70, 0x00, 0x00, 0x07, 0x01, 0x82, 0x8E, 0x0D, 0x0A
    };

    // 3. The intercepted STOP/RELEASE binary payload
    uint8_t jog_stop[] = {
        0x41, 0x54, 0x90, 0x07, 0xEB, 0xFC, 0x08, 0x05, 
        0x70, 0x00, 0x00, 0x07, 0x00, 0x7F, 0xFF, 0x0D, 0x0A
    };

    std::cout << "Executing forward jog for 1.5 seconds..." << std::endl;
    
    // In many QDD actuators, movement commands must be sent continuously 
    // as a watchdog safety mechanism. We will loop it for 1.5 seconds.
    for(int i = 0; i < 30; i++) {
        write(serial_port, jog_fwd, sizeof(jog_fwd));
        usleep(50000); // Send every 50ms (20Hz)
    }
    
    write(serial_port, jog_stop, sizeof(jog_stop));
    std::cout << "Movement complete. Motor safely stopped." << std::endl;

    close(serial_port);
    return 0;
}