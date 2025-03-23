#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "terminal.hpp"
#include "controls.hpp"
#include <vector>

using namespace term;

// Global variable to store the original terminal settings.
struct termios orig_termios;


// Debug print: prints each byte as two-digit hex.
void printHex(const char* buf, ssize_t n) {
    std::cout << "Raw event: ";
    for (ssize_t i = 0; i < n; ++i)
        // std::cout << std::hex << (int)(unsigned char)buf[i] << " ";
      if (buf[i]<32) std::cout << "{" << std::hex << (int)(unsigned char)buf[i] << "}";
      else std::cout << buf[i];
    std::cout << std::dec << "\n";
}


// Restore terminal settings on exit.
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // Disable mouse reporting.
    std::cout << "\033[?1003l" << "\033[?1006l";
}

// Put the terminal into raw mode.
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); // Disable echo, canonical mode, and signals.
    raw.c_iflag &= ~(IXON | ICRNL);         // Disable Ctrl-S/Ctrl-Q and carriage return conversion.
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Optional: Make STDIN non-blocking.
void setNonBlocking() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    enableRawMode();
    setNonBlocking();

    // Enable mouse reporting
    std::cout << "\033[?1003h" << "\033[?1006h" << std::flush;

    // Create terminal and calculate dimensions
    term::Terminal terminal;
    
    // Create editor that fills the entire screen
    term::Editor editor(0, 0, terminal.getWidth(), terminal.getHeight(), 4, true);
    
    // Check if a file name was provided as a command line argument
    if (argc > 1) {
        std::string fileName = argv[1];
        
        if (!editor.loadFromFile(fileName)) {
            editor.setText("");
        }
    }
    
    editor.setFocus(true);

    char buf[64];
    while (true) {
        terminal.clear();
        editor.draw(terminal);
        terminal.refresh();
        
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0) {
            term::SGREvent ev = term::Terminal::parseSGREvent(buf);
            if (buf[0] == 3) break;  // Ctrl-C to exit
            editor.processEvent(ev);
        }
        
        usleep(10000);  // Sleep 10ms
    }

    return 0;
}
