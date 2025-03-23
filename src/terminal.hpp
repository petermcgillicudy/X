#ifndef TERMINAL_HPP
#define TERMINAL_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <string>   // for std::u32string
#include <sys/ioctl.h>
#include <unistd.h>  // for STDOUT_FILENO

namespace term {

// --- Named Colors ---
enum Color {
    Default = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    Gray = 90
};

// --- Style Flag Bit Masks ---
const uint32_t STYLE_BOLD      = 1 << 0;
const uint32_t STYLE_UNDERLINE = 1 << 1;
// (Add more flags as needed)



// Define an enum for mouse buttons and wheel events.
enum class ButtonPressed {
    None,
    Left,
    Middle,
    Right,
    Release,
    WheelUp,
    WheelDown
};

// Define key codes for special keys
enum class KeyCode {
    None,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    Tab,
    Enter,
    Escape,
    Backspace
};

struct SGREvent {
    bool isMouseEvent;
    bool isSpecial;     // True if this is a multi-character special key sequence.
    bool ctrl;          // True if Ctrl was pressed.
    bool alt;           // True if Alt was pressed.
    bool shift;         // True if Shift was pressed.
    char key;           // For normal key events or special key identifier.
    KeyCode keyCode;    // For special keys.
    std::string special; // The full special sequence, if isSpecial is true.
    std::string fullSpecial; // The full special sequence, if isSpecial is true.
    ButtonPressed button; // For mouse events.
    int x, y;           // Mouse coordinates.
    int wheel;          // For wheel events: 1 for up, -1 for down.

    SGREvent()
      : isMouseEvent(false), isSpecial(false), ctrl(false), alt(false), shift(false),
        key(0), keyCode(KeyCode::None), button(ButtonPressed::None), x(0), y(0), wheel(0) {}
};

// --- OutputBuffer Object ---
// A dynamically growing output buffer that persists between refreshes.
class OutputBuffer {
public:
    OutputBuffer(size_t initialCapacity = 512)
        : length(0), capacity(initialCapacity)
    {
        data = (char*) malloc(capacity);
        if (data)
            data[0] = '\0';
    }
    ~OutputBuffer() {
        if (data)
            free(data);
    }
    // Clears the buffer for reuse.
    void clear() {
        length = 0;
        if (data)
            data[0] = '\0';
    }
    // Append a C-string.
    void append(const char* s) {
        size_t len = strlen(s);
        ensureCapacity(len);
        memcpy(data + length, s, len);
        length += len;
        data[length] = '\0';
    }
    // Append a single character.
    void appendChar(char c) {
        ensureCapacity(1);
        data[length++] = c;
        data[length] = '\0';
    }
    // Append formatted text.
    void appendFormat(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int required = vsnprintf(NULL, 0, fmt, args);
        va_end(args);
        if (required < 0)
            return;
        ensureCapacity((size_t) required);
        va_start(args, fmt);
        vsnprintf(data + length, capacity - length, fmt, args);
        va_end(args);
        length += (size_t) required;
    }
    const char* c_str() const {
        return data;
    }
    // Add this method to handle resizing
    void resize(size_t newCapacity) {
        if (newCapacity > capacity) {
            data = (char*) realloc(data, newCapacity);
            capacity = newCapacity;
        }
        length = 0;
        if (data) {
            data[0] = '\0';
        }
    }
private:
    char* data;
    size_t length;
    size_t capacity;
    // Increases capacity if necessary.
    void ensureCapacity(size_t extra) {
        if (length + extra >= capacity) {
            size_t newCapacity = capacity * 2;
            while (length + extra >= newCapacity)
                newCapacity *= 2;
            data = (char*) realloc(data, newCapacity);
            capacity = newCapacity;
        }
    }
};

// --- Minimal UTF-8 Decoder ---
// Converts a UTF-8 encoded C-string into a std::u32string (sequence of Unicode code points).
// This implementation handles ASCII and common multi-byte sequences (enough for many Western languages).
inline std::u32string decode_utf8(const char* str) {
    std::u32string result;
    while (*str) {
        unsigned char c = (unsigned char)*str;
        char32_t code_point = 0;
        int num_bytes = 0;
        
        if (c < 0x80) {
            code_point = c;
            num_bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            code_point = c & 0x1F;
            code_point = (code_point << 6) | (((unsigned char)str[1]) & 0x3F);
            num_bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            code_point = c & 0x0F;
            code_point = (code_point << 6) | (((unsigned char)str[1]) & 0x3F);
            code_point = (code_point << 6) | (((unsigned char)str[2]) & 0x3F);
            num_bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            code_point = c & 0x07;
            code_point = (code_point << 6) | (((unsigned char)str[1]) & 0x3F);
            code_point = (code_point << 6) | (((unsigned char)str[2]) & 0x3F);
            code_point = (code_point << 6) | (((unsigned char)str[3]) & 0x3F);
            num_bytes = 4;
        } else {
            // Invalid or unsupported byte sequence; treat as ASCII
            code_point = c;
            num_bytes = 1;
        }
        
        result.push_back(code_point);
        str += num_bytes;
    }
    return result;
}

// --- Unicode Encoding Helper ---
// Converts a Unicode code point (char32_t) into UTF-8 and appends it to the output buffer.
inline void encode_utf8(char32_t cp, OutputBuffer &buf) {
    if (cp < 0x80) {
        buf.appendChar(static_cast<char>(cp));
    } else if (cp < 0x800) {
        char first = static_cast<char>(0xC0 | (cp >> 6));
        char second = static_cast<char>(0x80 | (cp & 0x3F));
        buf.appendChar(first);
        buf.appendChar(second);
    } else if (cp < 0x10000) {
        char first = static_cast<char>(0xE0 | (cp >> 12));
        char second = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        char third = static_cast<char>(0x80 | (cp & 0x3F));
        buf.appendChar(first);
        buf.appendChar(second);
        buf.appendChar(third);
    } else if (cp <= 0x10FFFF) {
        char first = static_cast<char>(0xF0 | (cp >> 18));
        char second = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        char third = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        char fourth = static_cast<char>(0x80 | (cp & 0x3F));
        buf.appendChar(first);
        buf.appendChar(second);
        buf.appendChar(third);
        buf.appendChar(fourth);
    }
}

// --- ExChar: Extended Character with Style ---
// Now each ExChar stores a Unicode code point in a fixed-width type.
struct ExChar {
    char32_t ch;      // Unicode code point
    Color foreground;
    Color background;
    uint32_t styleFlags;

    ExChar(char32_t c = U' ',
           Color fg = Default,
           Color bg = Default,
           uint32_t style = 0)
        : ch(c), foreground(fg), background(bg), styleFlags(style)
    {}

    // Compares only style (colors and flags) ignoring the character.
    bool sameStyle(const ExChar &other) const {
        return (foreground == other.foreground) &&
               (background == other.background) &&
               (styleFlags == other.styleFlags);
    }

    // Generates a full ANSI escape sequence for this ExChar's style.
    void generateEscape(OutputBuffer &buf) const {
        buf.append("\033[");
        bool first = true;
        if (styleFlags & STYLE_BOLD) {
            buf.append("1");
            first = false;
        }
        if (styleFlags & STYLE_UNDERLINE) {
            if (!first) buf.append(";");
            buf.append("4");
            first = false;
        }
        if (!first)
            buf.append(";");
        buf.append(getForegroundCode(foreground));
        buf.append(";");
        buf.append(getBackgroundCode(background));
        buf.append("m");
    }

    // Generates a minimal ANSI escape sequence to change style from 'from' to this style.
    void diffEscape(const ExChar &from, OutputBuffer &buf) const {
        if (this->sameStyle(from))
            return;
        buf.append("\033[");
        bool first = true;
        if (from.foreground != foreground) {
            buf.append(getForegroundCode(foreground));
            first = false;
        }
        if (from.background != background) {
            if (!first) buf.append(";");
            buf.append(getBackgroundCode(background));
            first = false;
        }
        bool fromBold = (from.styleFlags & STYLE_BOLD) != 0;
        bool toBold = (styleFlags & STYLE_BOLD) != 0;
        if (fromBold != toBold) {
            if (!first) buf.append(";");
            buf.append(toBold ? "1" : "22"); // 22 resets bold/dim.
            first = false;
        }
        bool fromUnder = (from.styleFlags & STYLE_UNDERLINE) != 0;
        bool toUnder = (styleFlags & STYLE_UNDERLINE) != 0;
        if (fromUnder != toUnder) {
            if (!first) buf.append(";");
            buf.append(toUnder ? "4" : "24"); // 24 turns off underline.
            first = false;
        }
        buf.append("m");
    }
private:
    // ANSI Color Code Helpers as private static methods.
    static const char* getForegroundCode(Color c) {
        switch(c) {
            case Black:   return "30";
            case Red:     return "31";
            case Green:   return "32";
            case Yellow:  return "33";
            case Blue:    return "34";
            case Magenta: return "35";
            case Cyan:    return "36";
            case White:   return "37";
            case Default:
            default:      return "39";
        }
    }
    static const char* getBackgroundCode(Color c) {
        switch(c) {
            case Black:   return "40";
            case Red:     return "41";
            case Green:   return "42";
            case Yellow:  return "43";
            case Blue:    return "44";
            case Magenta: return "45";
            case Cyan:    return "46";
            case White:   return "47";
            case Default:
            default:      return "49";
        }
    }
};

// Equality operator for ExChar (compares both the code point and its style).
inline bool operator==(const ExChar &a, const ExChar &b) {
    return (a.ch == b.ch) &&
           (a.foreground == b.foreground) &&
           (a.background == b.background) &&
           (a.styleFlags == b.styleFlags);
}

// --- Terminal Class ---
// Implements double buffering and builds output in one shot.
class Terminal {
public:
    // Default constructor - auto-detects size
    Terminal() : Terminal(0, 0) {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            m_width = w.ws_col;
            m_height = w.ws_row;
            // Resize buffer for actual terminal size
            m_buf.resize(4 * (size_t)m_width * m_height);
            
            size_t size = (size_t)m_width * m_height;
            delete[] m_frontBuffer;
            delete[] m_backBuffer;
            m_frontBuffer = new ExChar[size];
            m_backBuffer  = new ExChar[size];
            for (size_t i = 0; i < size; ++i) {
                m_frontBuffer[i] = ExChar(U' ', Default, Default, 0);
                m_backBuffer [i] = ExChar(U' ', Default, Default, 0);
            }
        }
    }

    // Explicit size constructor
    Terminal(int width, int height) 
        : m_width(width), m_height(height),
          m_buf(4 * (size_t)width * height)
    {
        size_t size = (size_t)m_width * m_height;
        m_frontBuffer = new ExChar[size];
        m_backBuffer  = new ExChar[size];
        for (size_t i = 0; i < size; ++i) {
            m_frontBuffer[i] = ExChar(U' ', Default, Default, 0);
            m_backBuffer [i] = ExChar(U' ', Default, Default, 0);
        }
        // Hide the cursor and clear the screen
        fputs("\033[?25l\033[2J", stdout);
    }

    ~Terminal() {
        // Reset terminal (clear styles and show the cursor).
        fputs("\033[0m\033[?25h", stdout);
        delete[] m_frontBuffer;
        delete[] m_backBuffer;
    }

    // Clears the front buffer.
    void clear() {
        size_t size = (size_t) m_width * m_height;
        for (size_t i = 0; i < size; ++i)
            m_frontBuffer[i] = ExChar(U' ', Default, Default, 0);
    }

    // Places an ExChar at (x, y) in the front buffer.
    void putChar(int x, int y, const ExChar &ec) {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height)
            m_frontBuffer[y * m_width + x] = ec;
    }

    // Places a string at (x, y) in the front buffer with the given style
    void putString(int x, int y, const char* str, Color fg = Default, Color bg = Default, uint32_t style = 0) {
        std::u32string codepoints = decode_utf8(str);
        int currentX = x;
        
        for (char32_t cp : codepoints) {
            if (currentX >= 0 && currentX < m_width && y >= 0 && y < m_height) {
                m_frontBuffer[y * m_width + currentX] = ExChar(cp, fg, bg, style);
                currentX++;
            }
        }
    }

    // Overload for std::string
    void putString(int x, int y, const std::string& str, Color fg = Default, Color bg = Default, uint32_t style = 0) {
        putString(x, y, str.c_str(), fg, bg, style);
    }

    // Refreshes the terminal by comparing buffers, building one output string, and flushing it.
    void refresh() {
        m_buf.clear();
        ExChar currentStyle(U' ', Default, Default, 0);
        // Reset terminal state
        m_buf.append("\033[0m");
        int lastX = -1, lastY = -1;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                size_t idx = (size_t) y * m_width + x;
                ExChar &front = m_frontBuffer[idx];
                ExChar &back  = m_backBuffer[idx];
                if (!(front == back)) {
                    // Only move the cursor if the new cell is not immediately adjacent.
                    if (!(lastY == y && lastX == x - 1)) {
                        m_buf.appendFormat("\033[%d;%dH", y + 1, x + 1);
                    }
                    // Update style if needed.
                    if (!currentStyle.sameStyle(front)) {
                        front.diffEscape(currentStyle, m_buf);
                        currentStyle = front;
                    }
                    // Instead of appending a char directly, encode the Unicode code point to UTF-8.
                    encode_utf8(front.ch, m_buf);
                    lastX = x;
                    lastY = y;
                    // Update the back buffer.
                    m_backBuffer[idx] = front;
                }
            }
        }
        fputs(m_buf.c_str(), stdout);
        fflush(stdout);
    }

    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

     
// Parses an input string (an escape sequence) and returns an SGREvent.
    static inline SGREvent parseSGREvent(const std::string &input) {
        SGREvent ev;
        // Initialize with default values
        ev.isMouseEvent = false;
        ev.button = ButtonPressed::None;
        ev.x = ev.y = ev.wheel = 0;
        ev.alt = false;
        ev.ctrl = false;
        ev.shift = false;
        ev.isSpecial = false;
        ev.special = "";
        ev.fullSpecial = input;  // Store the full input sequence
        ev.keyCode = KeyCode::None;

        if (input.empty()) {
            return ev;
        }

        // Check for backspace 
        if (!input.empty() && (input[0] == 8 || input[0] == 127)) {
            ev.keyCode = KeyCode::Backspace;
            return ev;
        }

        // First, try to extract a valid escape sequence if present
        std::string relevantInput = input;
        if (input[0] == '\033' && input.size() >= 3) {
            // Find the end of the escape sequence
            size_t escEnd = 2;  // Start after ESC [
            while (escEnd < input.size()) {
                char c = input[escEnd];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~') {
                    escEnd++;  // Include the terminating character
                    break;
                }
                escEnd++;
            }
            
            // Extract just the escape sequence
            if (escEnd <= input.size()) {
                relevantInput = input.substr(0, escEnd);
            }
        }

        // Now process the relevant input
        if (relevantInput.size() >= 3 && relevantInput.substr(0, 3) == "\033[<") {
            // Mouse event processing
            ev.isMouseEvent = true;
            // Find the final letter (should be either 'M' for press/motion or 'm' for release)
            size_t pos = relevantInput.find_last_of("Mm");
            if (pos == std::string::npos)
                return ev; // Invalid sequence; return default event.

            char finalLetter = relevantInput[pos];
            // The content between "<" and the final letter is something like "35;117;33"
            std::string content = relevantInput.substr(3, pos - 3); // skip "\033[<"
            int code = 0;
            if (sscanf(content.c_str(), "%d;%d;%d", &code, &ev.x, &ev.y) != 3)
                return ev; // Parsing error.

            // For mouse events, check if modifiers were pressed
            ev.shift = (code & 4) != 0;
            ev.alt = (code & 8) != 0;
            ev.ctrl = (code & 16) != 0;

            if (finalLetter == 'm') {
                ev.button = ButtonPressed::Release;
            } else {
                // Handle wheel events (code 64 for wheel up, 65 for wheel down).
                if (code == 64) {
                    ev.button = ButtonPressed::WheelUp;
                    ev.wheel = 1;
                } else if (code == 65) {
                    ev.button = ButtonPressed::WheelDown;
                    ev.wheel = -1;
                } else {
                    // For normal mouse events, the lower bits indicate the button:
                    // Typically: 0 = left, 1 = middle, 2 = right.
                    int btn = code & 0x03;
                    switch (btn) {
                        case 0: ev.button = ButtonPressed::Left; break;
                        case 1: ev.button = ButtonPressed::Middle; break;
                        case 2: ev.button = ButtonPressed::Right; break;
                        default: ev.button = ButtonPressed::None; break;
                    }
                }
            }
        }
        else if (relevantInput[0] == '\033' && relevantInput.size() >= 3 && relevantInput[1] == '[') {
            // This is a CSI sequence
            ev.isSpecial = true;
            
            // Store the special sequence for reference
            ev.special = relevantInput;
            
            // Check for common cursor key sequences
            if (relevantInput == "\033[A") {
                ev.keyCode = KeyCode::Up;
            } else if (relevantInput == "\033[B") {
                ev.keyCode = KeyCode::Down;
            } else if (relevantInput == "\033[C") {
                ev.keyCode = KeyCode::Right;
            } else if (relevantInput == "\033[D") {
                ev.keyCode = KeyCode::Left;
            } else if (relevantInput == "\033[H" || relevantInput == "\033[1~") {
                ev.keyCode = KeyCode::Home;
            } else if (relevantInput == "\033[F" || relevantInput == "\033[4~") {
                ev.keyCode = KeyCode::End;
            } else if (relevantInput == "\033[2~") {
                ev.keyCode = KeyCode::Insert;
            } else if (relevantInput == "\033[3~") {
                ev.keyCode = KeyCode::Delete;
            } else if (relevantInput == "\033[5~") {
                ev.keyCode = KeyCode::PageUp;
            } else if (relevantInput == "\033[6~") {
                ev.keyCode = KeyCode::PageDown;
            } else if (relevantInput == "\033[Z") {
                ev.keyCode = KeyCode::Tab;
                ev.shift = true;
            } else {
                // Check for sequences with modifiers
                size_t semicolonPos = relevantInput.find(';', 2);
                if (semicolonPos != std::string::npos && semicolonPos + 1 < relevantInput.size()) {
                    // Extract the modifier part
                    std::string modifierStr = relevantInput.substr(semicolonPos + 1);
                    size_t modifierEnd = modifierStr.find_first_not_of("0123456789");
                    if (modifierEnd != std::string::npos) {
                        modifierStr = modifierStr.substr(0, modifierEnd);
                    }
                    
                    try {
                        int modifier = std::stoi(modifierStr);
                        
                        // Parse modifier bits
                        ev.shift = (modifier == 2 || modifier == 4 || modifier == 6 || modifier == 8);
                        ev.alt = (modifier == 3 || modifier == 4 || modifier == 7 || modifier == 8);
                        ev.ctrl = (modifier == 5 || modifier == 6 || modifier == 7 || modifier == 8);
                        
                        // Check the base key
                        char finalChar = relevantInput.back();
                        if (finalChar == 'A') ev.keyCode = KeyCode::Up;
                        else if (finalChar == 'B') ev.keyCode = KeyCode::Down;
                        else if (finalChar == 'C') ev.keyCode = KeyCode::Right;
                        else if (finalChar == 'D') ev.keyCode = KeyCode::Left;
                        else if (finalChar == 'H') ev.keyCode = KeyCode::Home;
                        else if (finalChar == 'F') ev.keyCode = KeyCode::End;
                        else if (finalChar == '~') {
                            // Extract the number before the semicolon
                            std::string numStr = relevantInput.substr(2, semicolonPos - 2);
                            try {
                                int num = std::stoi(numStr);
                                switch (num) {
                                    case 1: ev.keyCode = KeyCode::Home; break;
                                    case 2: ev.keyCode = KeyCode::Insert; break;
                                    case 3: ev.keyCode = KeyCode::Delete; break;
                                    case 4: ev.keyCode = KeyCode::End; break;
                                    case 5: ev.keyCode = KeyCode::PageUp; break;
                                    case 6: ev.keyCode = KeyCode::PageDown; break;
                                }
                            } catch (...) {
                                // Failed to parse number
                            }
                        }
                    } catch (...) {
                        // Failed to parse modifier
                    }
                }
            }
        }
        else if (relevantInput[0] == '\033' && relevantInput.size() >= 2) {
            // Alt + key combination
            ev.alt = true;
            char c = relevantInput[1];
            if (c < 32) {
                // Alt + Ctrl + key
                ev.ctrl = true;
                ev.key = (c - 1) + 'a';
            } else {
                ev.key = c;
            }
        }
        else {
            // Normal character input
            char c = relevantInput[0];
            if (c < 32) {
                if (c == 13 || c == 10) {
                    // Enter key
                    ev.keyCode = KeyCode::Enter;
                } else if (c == 9) {
                    // Tab key
                    ev.keyCode = KeyCode::Tab;
                } else if (c == 27) {
                    // Escape key
                    ev.keyCode = KeyCode::Escape;
                } else {
                    // Control character
                    ev.ctrl = true;
                    ev.key = (c - 1) + 'a';
                }
            } else {
                ev.key = c;
            }
        }
        
        return ev;
    }

private:
    int m_width;
    int m_height;
    ExChar* m_frontBuffer;
    ExChar* m_backBuffer;
    OutputBuffer m_buf;
};






} // namespace term

#endif // TERMINAL_HPP
