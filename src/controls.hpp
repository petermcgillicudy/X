#pragma once

#include "terminal.hpp"
#include <string>
#include <vector>
#include <memory>
#include <algorithm> 
#include <chrono>
#include <fstream>

namespace term {
    


// Abstract base class for selections
class Selection {
public:
    virtual ~Selection() = default;
    
    // Check if a position is within the selection
    virtual bool contains(size_t flatPos) const = 0;
    
    // Get the start and end positions of the selection
    virtual size_t getStart() const = 0;
    virtual size_t getEnd() const = 0;
    
    // Clone the selection
    virtual Selection* clone() const = 0;
};


// Simple range selection (from start to end position)
class RangeSelection : public Selection {
private:
    size_t m_start;
    size_t m_end;
    
public:
    RangeSelection(size_t start, size_t end) 
        : m_start(start), m_end(end) {
        // Ensure start <= end
        if (m_start > m_end) {
            std::swap(m_start, m_end);
        }
    }

    void fix() {
        if (m_start > m_end) {
            std::swap(m_start, m_end);
        }
    }

    void update(size_t start, size_t end) {
        m_start = start;
        m_end = end;
    }
    
    bool contains(size_t flatPos) const override {
        if (m_start <= m_end)
            return flatPos >= m_start && flatPos < m_end;
        else
            return flatPos >= m_end && flatPos < m_start;
    }
    
    size_t getStart() const override { return m_start; }
    size_t getEnd() const override { return m_end; }
    
    Selection* clone() const override {
        return new RangeSelection(m_start, m_end);
    }
};


class Control {
protected:
    int m_x, m_y;
    int m_width, m_height;
    bool m_hasFocus;

public:
    Control(int x, int y, int width = 1, int height = 1) 
        : m_x(x), m_y(y), m_width(width), m_height(height), m_hasFocus(false) {}
    virtual ~Control() = default;

    virtual bool processEvent(const SGREvent& ev) = 0;
    virtual void setFocus(bool focus) { m_hasFocus = focus; }
    bool hasFocus() const { return m_hasFocus; }
    

    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }
    void setWidth(int width) { m_width = width; }
    void setHeight(int height) { m_height = height; }
    void setPosition(int x, int y) { m_x = x; m_y = y; }
    void setSize(int width, int height) { m_width = width; m_height = height; }
    
    virtual void draw(Terminal& term) = 0;
};

class ScrollBar : public Control {
private:
    size_t m_totalSize;      // Total document size (in lines)
    size_t m_visibleSize;    // Visible area size (in lines)
    size_t m_position;       // Current position (in lines from top)
    Color m_barColor;        // Color of the scrollbar thumb
    Color m_backgroundColor; // Color of the scrollbar background
    
    // Block characters for different heights (bottom-up)
    static constexpr char32_t BLOCKS[] = {
        U' ',      // 0/8 (empty)
        U'▁',      // 1/8
        U'▂',      // 2/8
        U'▃',      // 3/8
        U'▄',      // 4/8
        U'▅',      // 5/8
        U'▆',      // 6/8
        U'▇',      // 7/8
        U'█'       // 8/8 (full)
    };
    
    void drawChar(Terminal& term, int x, int y, int eighths, bool inverted) {
        if (inverted && eighths > 0 && eighths < 8) {
            // For bottom edge, invert colors and use complementary block size
            term.putChar(x, y, ExChar(BLOCKS[8 - eighths], 
                                    m_backgroundColor, m_barColor, 0));
        } else if (eighths == 8) {
            // Full block - use bar color
            term.putChar(x, y, ExChar(BLOCKS[eighths], 
                                    m_barColor,  
                                    m_barColor, 
                                    0));
        } else if (eighths > 0) {
            // Top edge fractional block - invert colors like bottom edge
            term.putChar(x, y, ExChar(BLOCKS[eighths], 
                                    m_barColor, m_backgroundColor, 0));
        } else {
            // Empty block - use background color
            term.putChar(x, y, ExChar(BLOCKS[0], 
                                    Default,
                                    m_backgroundColor, 
                                    0));
        }
    }
    
public:
    ScrollBar(int x, int y, int height, Color barColor = Yellow, Color bgColor = Blue) 
        : Control(x, y, 1, height)
        , m_totalSize(0)
        , m_visibleSize(0)
        , m_position(0)
        , m_barColor(barColor)
        , m_backgroundColor(bgColor) {}
    
    void setMetrics(size_t totalSize, size_t visibleSize, size_t position) {
        m_totalSize = totalSize;
        m_visibleSize = std::min(visibleSize, totalSize);
        m_position = std::min(position, totalSize > visibleSize ? totalSize - visibleSize : 0);
    }
    
    void setBarColor(Color color) { m_barColor = color; }
    void setBackgroundColor(Color color) { m_backgroundColor = color; }
    
    void draw(Terminal& term) override {
        if (m_totalSize == 0 || m_visibleSize == 0) return;
        
        // Calculate exact thumb size and position
        float viewRatio = static_cast<float>(m_visibleSize) / m_totalSize;
        float posRatio = static_cast<float>(m_position) / (m_totalSize - m_visibleSize);
        
        float exactThumbSize = m_height * viewRatio;
        float exactThumbPos = (m_height - exactThumbSize) * posRatio;
        
        // Calculate the integer and fractional parts
        int thumbStart = static_cast<int>(exactThumbPos);
        int thumbEnd = static_cast<int>(exactThumbPos + exactThumbSize);
        
        // Calculate fractional blocks for start and end
        int startFraction = static_cast<int>((1.0f - (exactThumbPos - thumbStart)) * 8);
        int endFraction = static_cast<int>((exactThumbPos + exactThumbSize - thumbEnd) * 8);
        
        // Draw the scrollbar
        for (int i = 0; i < m_height; i++) {
            if (i < thumbStart || i > thumbEnd) {
                // Outside thumb area
                drawChar(term, m_x, m_y + i, 0, false);
            } else if (i == thumbStart && i == thumbEnd) {
                // Single-character thumb
                drawChar(term, m_x, m_y + i, 
                        static_cast<int>(exactThumbSize * 8), false);
            } else if (i == thumbStart) {
                // Top edge
                drawChar(term, m_x, m_y + i, startFraction, false);
            } else if (i == thumbEnd) {
                // Bottom edge
                drawChar(term, m_x, m_y + i, endFraction, true);
            } else {
                // Middle of thumb
                drawChar(term, m_x, m_y + i, 8, false);
            }
        }
    }

    bool processEvent(const SGREvent& ev) override {
        // ScrollBar doesn't process any events
        return false;
    }
};

class UndoableEdit : public Control {
public:
    // Command base class for undo/redo operations
    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute(UndoableEdit& control) = 0;
        virtual void undo(UndoableEdit& control) = 0;
        
        // Return the memory size of this command (in bytes)
        virtual size_t getSize() const = 0;
    };

    // Text operation commands
    class InsertTextCommand : public Command {
    private:
        size_t m_pos;
        std::string m_text;
        
    public:
        InsertTextCommand(size_t pos, const std::string& text)
            : m_pos(pos), m_text(text) {}
            
        void execute(UndoableEdit& control) override {
            control.insertTextInternal(m_pos, m_text);
        }
        
        void undo(UndoableEdit& control) override {
            control.deleteTextInternal(m_pos, m_text.length());
        }
        
        size_t getSize() const override {
            // Size of the command = fixed overhead + size of the text
            return sizeof(*this) + m_text.capacity() * sizeof(char);
        }
    };
    
    class DeleteTextCommand : public Command {
    private:
        size_t m_pos;
        std::string m_deletedText;
        
    public:
        DeleteTextCommand(size_t pos, const std::string& deletedText)
            : m_pos(pos), m_deletedText(deletedText) {}
            
        void execute(UndoableEdit& control) override {
            control.deleteTextInternal(m_pos, m_deletedText.length());
        }
        
        void undo(UndoableEdit& control) override {
            control.insertTextInternal(m_pos, m_deletedText);
        }
        
        size_t getSize() const override {
            // Size of the command = fixed overhead + size of the deleted text
            return sizeof(*this) + m_deletedText.capacity() * sizeof(char);
        }
    };
    
    class ReplaceTextCommand : public Command {
    private:
        size_t m_pos;
        std::string m_oldText;
        std::string m_newText;
        
    public:
        ReplaceTextCommand(size_t pos, const std::string& oldText, const std::string& newText)
            : m_pos(pos), m_oldText(oldText), m_newText(newText) {}
            
        void execute(UndoableEdit& control) override {
            control.deleteTextInternal(m_pos, m_oldText.length());
            control.insertTextInternal(m_pos, m_newText);
        }
        
        void undo(UndoableEdit& control) override {
            control.deleteTextInternal(m_pos, m_newText.length());
            control.insertTextInternal(m_pos, m_oldText);
        }
        
        size_t getSize() const override {
            // Size of the command = fixed overhead + size of both old and new text
            return sizeof(*this) +  
                   m_oldText.capacity() * sizeof(char) + 
                   m_newText.capacity() * sizeof(char);
        }
    };

protected:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    size_t m_maxUndoLevels;
    size_t m_maxUndoSize;  // Maximum size in bytes
    size_t m_currentUndoSize;  // Current size in bytes
    bool m_undoEnabled;  // New property
    
    // Constructor with undoEnabled parameter
    UndoableEdit(int x, int y, int width = 1, int height = 1, 
                size_t maxUndoLevels = 1000, size_t maxUndoSize = 1024 * 1024,
                bool undoEnabled = true)
        : Control(x, y, width, height), 
          m_maxUndoLevels(maxUndoLevels),
          m_maxUndoSize(maxUndoSize),
          m_currentUndoSize(0),
          m_undoEnabled(undoEnabled) {}
    
    // Helper method for command execution
    void executeCommand(std::unique_ptr<Command> command) {
        // Execute the command
        command->execute(*this);
        
        // Only add to undo stack if undo is enabled
        if (m_undoEnabled) {
            // Add its size to our tracking
            size_t commandSize = command->getSize();
            m_currentUndoSize += commandSize;
            
            // Add to undo stack
            m_undoStack.push_back(std::move(command));
            
            // Clear redo stack when a new command is executed
            while (!m_redoStack.empty()) {
                m_currentUndoSize -= m_redoStack.back()->getSize();
                m_redoStack.pop_back();
            }
            
            // Trim undo stack if it exceeds limits
            while ((!m_undoStack.empty()) && 
                  ((m_undoStack.size() > m_maxUndoLevels) || 
                   (m_currentUndoSize > m_maxUndoSize))) {
                m_currentUndoSize -= m_undoStack.front()->getSize();
                m_undoStack.erase(m_undoStack.begin());
            }
        }
    }
    
    // Virtual methods that derived classes must implement
    virtual void insertTextInternal(size_t pos, const std::string& text) = 0;
    virtual void deleteTextInternal(size_t pos, size_t length) = 0;
    virtual std::string getTextAt(size_t pos, size_t length) const = 0;
    virtual size_t getTextLength() const = 0;

public:
    // Getter and setter for undoEnabled
    bool isUndoEnabled() const { return m_undoEnabled; }
    
    void setUndoEnabled(bool enabled) { 
        m_undoEnabled = enabled; 
        if (!m_undoEnabled) {
            // Clear undo/redo stacks if undo is disabled
            clearUndoHistory();
        }
    }
    
    // Undo/redo methods - now check if undo is enabled
    bool canUndo() const { return m_undoEnabled && !m_undoStack.empty(); }
    bool canRedo() const { return m_undoEnabled && !m_redoStack.empty(); }
    
    void undo() {
        if (canUndo()) {
            auto command = std::move(m_undoStack.back());
            m_undoStack.pop_back();
            
            size_t commandSize = command->getSize();
            m_currentUndoSize -= commandSize;
            
            command->undo(*this);
            
            m_redoStack.push_back(std::move(command));
            m_currentUndoSize += commandSize;
        }
    }
    
    void redo() {
        if (canRedo()) {
            auto command = std::move(m_redoStack.back());
            m_redoStack.pop_back();
            
            size_t commandSize = command->getSize();
            m_currentUndoSize -= commandSize;
            
            command->execute(*this);
            
            m_undoStack.push_back(std::move(command));
            m_currentUndoSize += commandSize;
        }
    }
    
    // Clear undo/redo history
    void clearUndoHistory() {
        m_undoStack.clear();
        m_redoStack.clear();
        m_currentUndoSize = 0;
    }
    
    // Get current undo stack size in bytes
    size_t getUndoStackSize() const {
        return m_currentUndoSize;
    }
    
    // Get maximum allowed undo stack size in bytes
    size_t getMaxUndoSize() const {
        return m_maxUndoSize;
    }
    
    // Set maximum allowed undo stack size in bytes
    void setMaxUndoSize(size_t maxSize) {
        m_maxUndoSize = maxSize;
        
        // Trim stack if needed
        while (!m_undoStack.empty() && m_currentUndoSize > m_maxUndoSize) {
            m_currentUndoSize -= m_undoStack.front()->getSize();
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    
    // Public text editing methods that create commands
    void insertText(size_t pos, const std::string& text) {
        if (pos <= getTextLength()) {
            executeCommand(std::make_unique<InsertTextCommand>(pos, text));
        }
    }
    
    void deleteText(size_t pos, size_t length) {
        if (pos < getTextLength()) {
            std::string deletedText = getTextAt(pos, length);
            if (!deletedText.empty()) {
                executeCommand(std::make_unique<DeleteTextCommand>(pos, deletedText));
            }
        }
    }
    
    void replaceText(size_t pos, size_t length, const std::string& newText) {
        if (pos < getTextLength()) {
            std::string oldText = getTextAt(pos, length);
            executeCommand(std::make_unique<ReplaceTextCommand>(pos, oldText, newText));
        }
    }
    
    // Utility methods for text navigation
    static size_t findPrevWordStart(const std::string& text, size_t currentPos) {
        if (currentPos == 0) return 0;
        
        // Skip any whitespace before the current position
        size_t pos = currentPos;
        while (pos > 0 && isspace(text[pos-1])) pos--;
        
        // Find the start of the current word
        while (pos > 0 && !isspace(text[pos-1])) pos--;
        
        return pos;
    }
    
    static size_t findNextWordEnd(const std::string& text, size_t currentPos) {
        size_t len = text.length();
        if (currentPos >= len) return len;
        
        // Skip any whitespace after the current position
        size_t pos = currentPos;
        while (pos < len && isspace(text[pos])) pos++;
        
        // Find the end of the current word
        while (pos < len && !isspace(text[pos])) pos++;
        
        return pos;
    }
};

class Editor;  // Forward declaration

class EditBox : public UndoableEdit {
private:
    std::string m_text;
    size_t m_cursorPos;
    size_t m_leftIndex;
    int m_tabSize;
    Color m_backgroundColor;
    bool m_insertMode;
    Editor* m_parentEditor;  // Pointer to parent Editor

public:
    EditBox(int x, int y, int width, int tabSize = 4, bool undoEnabled = true, Editor* parentEditor = nullptr)
        : UndoableEdit(x, y, width, 1, 1000, 1024 * 1024, undoEnabled),
          m_text(""),
          m_cursorPos(0),
          m_leftIndex(0),
          m_tabSize(tabSize),
          m_backgroundColor(Default),
          m_insertMode(true),
          m_parentEditor(parentEditor) {}
    
    bool processEvent(const SGREvent& ev) override;
    void draw(Terminal& term) override;

    void toggleInsertMode() { m_insertMode = !m_insertMode; }
    
    void setTabSize(int tabSize) { m_tabSize = tabSize; }
    int getTabSize() const { return m_tabSize; }
    
    void setLeftIndex(size_t leftIndex) { 
        m_leftIndex = leftIndex;
        // Ensure cursor is visible
        if (m_cursorPos < m_leftIndex) {
            m_leftIndex = m_cursorPos;
        } else if (m_cursorPos >= m_leftIndex + m_width) {
            m_leftIndex = m_cursorPos - m_width + 1;
        }
    }
    
    size_t getLeftIndex() const { return m_leftIndex; }
    
    size_t getCursorPos() const { return m_cursorPos; }
    void setCursorPos(size_t newPos);

    void setBackgroundColor(Color color) { m_backgroundColor = color; }
    
    const std::string& getText() const { return m_text; }
    void setText(const std::string& text) {
        m_text = text;
        // setCursorPos(m_text.length());
    }
    
    // Set parent Editor
    void setParentEditor(Editor* editor) { m_parentEditor = editor; }
    
    // Get parent Editor
    Editor* getParentEditor() const { return m_parentEditor; }
    
    // Handle boundary conditions
    bool handleBoundary(bool isAtStart, bool isDelete);

    bool deleteParentSelectedText();


protected:
    // Insert text at the specified position
    void insertTextInternal(size_t pos, const std::string& text) override {
        if (pos <= m_text.length()) {
            m_text.insert(pos, text);
            setCursorPos(pos + text.length());
        }
    }
    
    // Delete text at the specified position
    void deleteTextInternal(size_t pos, size_t length) override {
        if (pos < m_text.length()) {
            size_t actualLength = std::min(length, m_text.length() - pos);
            m_text.erase(pos, actualLength);
            setCursorPos(pos);
        }
    }
    
    // Get text at the specified position
    std::string getTextAt(size_t pos, size_t length) const override {
        if (pos < m_text.length()) {
            return m_text.substr(pos, std::min(length, m_text.length() - pos));
        }
        return "";
    }
    
    // Get the total text length
    size_t getTextLength() const override {
        return m_text.length();
    }
};

class Label : public Control {
private:
    std::string m_text;
    Color m_textColor;
    Color m_backgroundColor;
    int m_style;  // Optional: for bold, underline, etc.

public:
    Label(int x, int y, int width, const std::string& text = "", 
          Color textColor = White, Color backgroundColor = Default, int style = 0)
        : Control(x, y, width, 1), 
          m_text(text), 
          m_textColor(textColor), 
          m_backgroundColor(backgroundColor),
          m_style(style) {}
    
    bool processEvent(const SGREvent& ev) override {
        // Labels don't process events
        return false;
    }
    
    void draw(Terminal& term) override {
        // Clear the label area with the background color
        for (int i = 0; i < m_width; i++) {
            term.putChar(m_x + i, m_y, ExChar(U' ', Default, m_backgroundColor, 0));
        }
        
        // Draw the text, truncated if necessary
        std::string visibleText = m_text;
        if (visibleText.length() > (size_t)m_width) {
            visibleText = visibleText.substr(0, m_width);
        }
        
        // Draw each character with the specified colors and style
        for (size_t i = 0; i < visibleText.length(); i++) {
            term.putChar(m_x + i, m_y, 
                        ExChar(visibleText[i], m_textColor, m_backgroundColor, m_style));
        }
    }
    
    // Getters and setters
    const std::string& getText() const { return m_text; }
    void setText(const std::string& text) { m_text = text; }
    
    Color getTextColor() const { return m_textColor; }
    void setTextColor(Color color) { m_textColor = color; }
    
    Color getBackgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(Color color) { m_backgroundColor = color; }
    
    int getStyle() const { return m_style; }
    void setStyle(int style) { m_style = style; }
};

class StatusBar : public Control {
private:
    int m_numLabels;
    Label** m_labels;  // Array of pointers to labels
    Color m_bgColor;
    
public:
    StatusBar(int x, int y, int width, int numLabels, const int* widths, 
              Color textColor = White, Color bgColor = Blue) 
        : Control(x, y, width, 1),
          m_numLabels(numLabels),
          m_bgColor(bgColor)
    {
        // Allocate array of label pointers
        m_labels = new Label*[m_numLabels];
        
        // Create labels
        int currentX = 0;
        for (int i = 0; i < m_numLabels; i++) {
            m_labels[i] = new Label(currentX, m_y, widths[i], "", textColor, m_bgColor);
            currentX += widths[i];
        }
    }
    
    ~StatusBar() {
        // Clean up all labels
        for (int i = 0; i < m_numLabels; i++) {
            delete m_labels[i];
        }
        delete[] m_labels;
    }
    
    bool processEvent(const SGREvent& ev) override {
        // StatusBar itself doesn't process events
        return false;
    }
    
    void draw(Terminal& term) override {
        // Fill the entire bar with background color
        for (int i = 0; i < m_width; i++) {
            term.putChar(i, m_y, ExChar(U' ', Default, m_bgColor, 0));
        }
        
        // Draw each label
        for (int i = 0; i < m_numLabels; i++) {
            m_labels[i]->draw(term);
        }
    }
    
    // Access a specific label
    Label* getLabel(int index) {
        if (index >= 0 && index < m_numLabels) {
            return m_labels[index];
        }
        return nullptr;
    }
    
    // Update the text of a specific label
    void setLabelText(int index, const std::string& text) {
        if (index >= 0 && index < m_numLabels) {
            m_labels[index]->setText(text);
        }
    }
    
};

class Editor : public UndoableEdit {
private:
    std::vector<std::string> m_lines;  // Text content split by lines
    size_t m_cursorX, m_cursorY;       // Cursor position (column, row)
    size_t m_leftChar, m_topLine;      // Scroll position
    int m_tabSize;                     // Tab size in spaces
    StatusBar* m_statusBar;            // Pointer to status bar (if enabled)
    ScrollBar* m_scrollBar;            // Add ScrollBar member
    EditBox* m_editBox;                // EditBox for editing the current line
    
    int m_updateFrequency;       // How often to update (in events)
    bool m_editBoxChanged;       // Flag to track if EditBox content changed
    
    std::chrono::steady_clock::time_point m_lastUpdateTime;  // Last time we updated from EditBox
    int m_updateIntervalMs;                                 // Update interval in milliseconds

    Selection* m_selection;  // Current selection (nullptr if no selection)
    bool m_isSelecting;
    Color m_selectionFg;     // Foreground color for selected text
    Color m_selectionBg;     // Background color for selected text

    std::string m_clipboard;  // Store clipboard contents

    std::string m_fileName;  // Associated file name (can be empty)

    size_t m_preferredX = 0;  // Add this member variable

    int m_numLinesWheelScroll = 3;  // Default value

    Color getSelectionFg() const { return m_selectionFg; }
    Color getSelectionBg() const { return m_selectionBg; }

    void updateFromEditBox();
    void updateEditBoxFromCurrentLine();
    
    // Helper function to convert logical position to screen position
    int logicalToScreenPos(const std::string& line, size_t pos) const {
        int screenPos = 0;
        for (size_t i = 0; i < pos && i < line.length(); i++) {
            if (line[i] == '\t') {
                screenPos += m_tabSize - (screenPos % m_tabSize);
            } else {
                screenPos++;
            }
        }
        return screenPos;
    }
    
    // Helper function to convert screen position to logical position
    size_t screenToLogicalPos(const std::string& line, int screenPos) const {
        int currentScreenPos = 0;
        size_t i = 0;
        
        while (i < line.length() && currentScreenPos < screenPos) {
            if (line[i] == '\t') {
                currentScreenPos += m_tabSize - (currentScreenPos % m_tabSize);
            } else {
                currentScreenPos++;
            }
            
            if (currentScreenPos <= screenPos) {
                i++;
            }
        }
        
        return i;
    }

    // Helper method to convert flat position to line and column
    void flatPosToLinePos(size_t flatPos, size_t& line, size_t& linePos) const {
        line = 0;
        linePos = flatPos;
        
        for (size_t i = 0; i < m_lines.size(); i++) {
            if (linePos <= m_lines[i].length()) {
                line = i;
                return;
            }
            
            // Move to next line (add 1 for the newline character)
            linePos -= (m_lines[i].length() + 1);
        }
        
        // If we get here, position is beyond the end of the text
        line = m_lines.size() - 1;
        linePos = m_lines[line].length();
    }
    
    // Helper method to convert line and column to flat position
    size_t linePosToFlatPos(size_t line, size_t linePos) const {
        size_t flatPos = 0;
        
        for (size_t i = 0; i < line && i < m_lines.size(); i++) {
            flatPos += m_lines[i].length() + 1; // +1 for newline
        }
        
        if (line < m_lines.size()) {
            flatPos += std::min(linePos, m_lines[line].length());
        }
        
        return flatPos;
    }

    
    // Ensure cursor is visible by adjusting scroll position
    void ensureCursorVisible() {
        // Vertical scrolling
        if (m_cursorY < m_topLine) {
            m_topLine = m_cursorY;
            positionEditBox();
            updateEditBoxFromCurrentLine(); 
        } else if (m_cursorY >= m_topLine + m_height) {
            m_topLine = m_cursorY - m_height + 1;
            positionEditBox();
            updateEditBoxFromCurrentLine();
        }
        
        // Horizontal scrolling - need to convert to screen position
        if (m_cursorY < m_lines.size()) {
            int screenCursorX = logicalToScreenPos(m_lines[m_cursorY], m_cursorX);
            int screenLeftChar = logicalToScreenPos(m_lines[m_cursorY], m_leftChar);
            
            if (screenCursorX < screenLeftChar) {
                m_leftChar = m_cursorX;
            } else if (screenCursorX >= screenLeftChar + m_width) {
                // Find logical position that puts cursor at right edge
                m_leftChar = screenToLogicalPos(m_lines[m_cursorY], 
                                               screenCursorX - m_width + 1);
            }
        }
    }

    // Insert text at the specified position
    void insertTextInternal(size_t pos, const std::string& text) override {
        // Convert flat position to line and column
        size_t line, linePos;
        flatPosToLinePos(pos, line, linePos);
        
        // Handle multi-line insertion (text contains newlines)
        size_t start = 0;
        size_t newlinePos = text.find('\n');
        
        if (newlinePos == std::string::npos) {
            // Simple case: no newlines in the text
            if (line < m_lines.size()) {
                m_lines[line].insert(linePos, text);
            }
        } else {
            // Complex case: text contains newlines
            std::vector<std::string> newLines;
            
            // Split the text into lines
            while (newlinePos != std::string::npos) {
                newLines.push_back(text.substr(start, newlinePos - start));
                start = newlinePos + 1;
                newlinePos = text.find('\n', start);
            }
            
            // Add the last part
            newLines.push_back(text.substr(start));
            
            if (line < m_lines.size()) {
                // First line: combine with the start of the current line
                std::string firstPart = m_lines[line].substr(0, linePos);
                std::string lastPart = m_lines[line].substr(linePos);
                
                // Update the current line with the first part + first new line
                m_lines[line] = firstPart + newLines[0];
                
                // Insert the middle lines
                for (size_t i = 1; i < newLines.size() - 1; i++) {
                    m_lines.insert(m_lines.begin() + line + i, newLines[i]);
                }
                
                // Insert the last new line + the end of the current line
                m_lines.insert(m_lines.begin() + line + newLines.size() - 1, 
                              newLines.back() + lastPart);
            }
        }
        
        // Update cursor position if needed
        if (m_cursorY == line && m_cursorX >= linePos) {
            if (text.find('\n') == std::string::npos) {
                // No newlines, just adjust X position
                m_cursorX += text.length();
            } else {
                // Text contains newlines, move cursor to the end of inserted text
                int newlineCount = 0;
                for (size_t i = 0; i < text.length(); i++) {
                    if (text[i] == '\n') {
                        newlineCount++;
                    }
                }
                m_cursorY = line + newlineCount;
                size_t lastNewlinePos = text.length();
                for (int i = text.length() - 1; i >= 0; i--) {
                    if (text[i] == '\n') {
                        lastNewlinePos = i;
                        break;
                    }
                }
                size_t lastLineLength = text.length() - lastNewlinePos - 1;
                m_cursorX = lastLineLength;
            }
        }
        
        ensureCursorVisible();
    }
    
    // Delete text at the specified position
    void deleteTextInternal(size_t pos, size_t length) override {
        if (length == 0) return;
        
        // Convert start position to line and column
        size_t startLine, startLinePos;
        flatPosToLinePos(pos, startLine, startLinePos);
        
        // Convert end position to line and column
        size_t endLine, endLinePos;
        flatPosToLinePos(pos + length, endLine, endLinePos);
        
        if (startLine == endLine) {
            // Simple case: deletion within a single line
            if (startLine < m_lines.size()) {
                m_lines[startLine].erase(startLinePos, endLinePos - startLinePos);
            }
        } else {
            // Complex case: deletion spans multiple lines
            if (startLine < m_lines.size() && endLine < m_lines.size()) {
                // Keep the start of the first line and the end of the last line
                std::string firstPart = m_lines[startLine].substr(0, startLinePos);
                std::string lastPart = m_lines[endLine].substr(endLinePos);
                
                // Combine them into a single line
                m_lines[startLine] = firstPart + lastPart;
                
                // Remove the lines in between
                m_lines.erase(m_lines.begin() + startLine + 1, 
                             m_lines.begin() + endLine + 1);
            }
        }
        
        // Update cursor position if needed
        if (m_cursorY > endLine) {
            // Cursor is after the deleted region, adjust Y position
            m_cursorY -= (endLine - startLine);
        } else if (m_cursorY == endLine && m_cursorX >= endLinePos) {
            // Cursor is on the last deleted line after the deletion point
            m_cursorY = startLine;
            m_cursorX = startLinePos + (m_cursorX - endLinePos);
        } else if (m_cursorY == startLine && m_cursorX >= startLinePos) {
            // Cursor is on the first deleted line after the deletion point
            m_cursorX = startLinePos;
        }
        
        ensureCursorVisible();
    }
    
    void copyToClipboard() {
        if (hasSelection()) {
            // First sync any changes from EditBox
            updateFromEditBox();

            m_clipboard = getSelectedText();
        }
    }
    
    // Cut selected text to clipboard
    void cutToClipboard() {
        if (hasSelection()) {
            // First sync any changes from EditBox
            updateFromEditBox();


            // First copy to clipboard
            m_clipboard = getSelectedText();
            
            // Then delete the selection (this is already undoable)
            deleteSelection();
            updateEditBoxFromCurrentLine();
        }
    }
    
    // Paste clipboard contents at current position
    void pasteFromClipboard() {
        if (!m_clipboard.empty()) {
            // First sync any changes from EditBox
            updateFromEditBox();
            
            // Calculate flat position for insertion
            size_t flatPos = linePosToFlatPos(m_cursorY, m_cursorX);
            
            // Create undoable action for paste
            insertText(flatPos, m_clipboard);
            updateEditBoxFromCurrentLine();

            // Update cursor position after paste
            size_t line, pos;
            flatPosToLinePos(flatPos + m_clipboard.length(), line, pos);
            setCursorPos(pos, line);
        }
    }




    // Get text at the specified position
    std::string getTextAt(size_t pos, size_t length) const override {
        std::string result;
        
        // Convert start position to line and column
        size_t startLine, startLinePos;
        flatPosToLinePos(pos, startLine, startLinePos);
        
        // Convert end position to line and column
        size_t endLine, endLinePos;
        flatPosToLinePos(pos + length, endLine, endLinePos);
        
        if (startLine == endLine) {
            // Simple case: text within a single line
            if (startLine < m_lines.size()) {
                result = m_lines[startLine].substr(startLinePos, endLinePos - startLinePos);
            }
        } else {
            // Complex case: text spans multiple lines
            if (startLine < m_lines.size()) {
                // Add the first line portion
                result = m_lines[startLine].substr(startLinePos);
                
                // Add the middle lines
                for (size_t i = startLine + 1; i < endLine && i < m_lines.size(); i++) {
                    result += '\n' + m_lines[i];
                }
                
                // Add the last line portion
                if (endLine < m_lines.size()) {
                    result += '\n' + m_lines[endLine].substr(0, endLinePos);
                }
            }
        }
        
        return result;
    }
    
    // Get the total text length
    size_t getTextLength() const override {
        size_t length = 0;
        
        for (size_t i = 0; i < m_lines.size(); i++) {
            length += m_lines[i].length();
            
            // Add 1 for each newline character (except after the last line)
            if (i < m_lines.size() - 1) {
                length++;
            }
        }
        
        return length;
    }    
    
    // Position the EditBox at the current cursor line
    void positionEditBox() {
        if (m_editBox && m_cursorY < m_lines.size() && m_editBox->getY() != m_y + (m_cursorY - m_topLine)) {
            int editY = m_y + (m_cursorY - m_topLine);
                    
            m_editBox->setPosition(m_x, editY);
            m_editBox->setWidth(m_width);  // Ensure same width as editor
            m_editBox->setTabSize(m_tabSize);  // Ensure same tab size
            m_editBox->setText(m_lines[m_cursorY]);
            m_editBox->setCursorPos(m_cursorX);
            
            // Make sure the EditBox has focus when the Editor has focus
            m_editBox->setFocus(m_hasFocus);
            
            // Set the EditBox's left index to match the editor's horizontal scroll
            m_editBox->setLeftIndex(m_leftChar);            
        }
    }

public:
    Editor(int x, int y, int width, int height, int tabSize = 4, bool hasStatusBar = false)
        : UndoableEdit(x, y, width - 1, height, 1000, 1024 * 1024, true),
          m_cursorX(0), m_cursorY(0),
          m_leftChar(0), m_topLine(0),
          m_tabSize(tabSize),
          m_statusBar(nullptr),
          m_scrollBar(nullptr),
          m_editBox(nullptr),
          m_updateFrequency(10),    // Default: update every 100 events
          m_editBoxChanged(false),
          m_lastUpdateTime(std::chrono::steady_clock::now()),
          m_updateIntervalMs(1000),
          m_selection(nullptr),
          m_isSelecting(false),
          m_selectionFg(White),
          m_selectionBg(Red)
    {
        // Initialize with an empty line
        m_lines.push_back("");
        
        // Create the EditBox for editing the current line
        m_editBox = new EditBox(x, y, width - 1, tabSize, false, this);  // Reduce width by 1
        m_editBox->setBackgroundColor(Default);
        m_editBox->setFocus(m_hasFocus);
        
        // Create ScrollBar on the right side of the editor
        m_scrollBar = new ScrollBar(x + width - 1, y, height, Yellow, Blue);
        
        if (hasStatusBar) {
            // Adjust editor height to make room for status bar
            m_height--;
            
            // Create status bar below the editor
            int statusWidths[] = {(width-1)/3, (width-1)/3, (width-1)/3};  // Adjust status bar width
            m_statusBar = new StatusBar(x, y + m_height, width - 1, 3, statusWidths);
            
            // Set initial status bar text
            m_statusBar->setLabelText(0, "Editor");
            m_statusBar->setLabelText(1, "");
            updateCursorInfo(); // This will set the 3rd label
        }
        
        // Position the EditBox initially
        positionEditBox();
    }
    
    ~Editor() {
        if (m_statusBar) {
            delete m_statusBar;
        }
        if (m_scrollBar) {
            delete m_scrollBar;
        }
        if (m_editBox) {
            delete m_editBox;
        }
        
        // Clean up selection
        if (m_selection) {
            delete m_selection;
        }
    }
    
    // Update the cursor position info in the status bar
    void updateCursorInfo() {
        if (m_statusBar) {
            char posInfo[30];
            sprintf(posInfo, "Line: %zu Col: %zu", m_cursorY + 1, m_cursorX + 1);
            m_statusBar->setLabelText(2, posInfo);
        }
    }
    
    bool processEvent(const SGREvent& ev) override {
        // Check if this is a selection-related event

        // First let the status bar process the event if it exists
        if (m_statusBar && m_statusBar->processEvent(ev)) {
            return true;
        }


        if (!ev.isMouseEvent && ev.ctrl) {
            if (ev.key == 'c' || ev.key == 'C') {
                copyToClipboard();
                return true;
            }
            if (ev.key == 'x' || ev.key == 'X') {
                cutToClipboard();
                return true;
            }
            if (ev.key == 'v' || ev.key == 'V') {
                pasteFromClipboard();
                return true;
            }
        }
       
        bool forceUpdate = false;
        
        // Process the event normally
        bool handled = false;

        if (!ev.isMouseEvent) {
            m_isSelecting = ev.shift;
            if (m_isSelecting && !hasSelection()) {
                startSelection();
            }
        }



        if (!m_hasFocus) return false;

        m_editBox->setFocus(true);
        
        // Let the EditBox process the event first
        handled = m_editBox->processEvent(ev);
        if (handled) {
            // Mark that EditBox content has changed
            m_editBoxChanged = true;

        }

        // Periodic update based on event counter
        
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_lastUpdateTime).count();
        
        if (elapsedTime >= m_updateIntervalMs) {
            forceUpdate = true;
            m_lastUpdateTime = currentTime;
        }
            

        // Update from EditBox if needed
        if (forceUpdate) {

            updateFromEditBox();
            m_editBoxChanged = false;
        }

        

        // Handle undo/redo keyboard shortcuts
        if (ev.ctrl && ev.key == 'z') {  // Ctrl+Z for undo
            undo();
            updateEditBoxFromCurrentLine();  // Sync EditBox after undo
            return true;
        }
        if (ev.ctrl && ev.key == 'y') {  // Ctrl+Y for redo
            redo();
            updateEditBoxFromCurrentLine();  // Sync EditBox after redo
            return true;
        }
        // Handle clipboard operations
    
        
        
        
        // Handle save keyboard shortcut
        if (ev.ctrl && ev.key == 's') {  // Ctrl+S for save
            bool saved = save();
            
            // Update status bar if available
            if (m_statusBar) {
                if (saved) {
                    m_statusBar->setLabelText(0, "Saved: " + m_fileName);
                } else {
                    m_statusBar->setLabelText(0, "Error saving file!");
                }
            }
            
            return true;
        }
        
        // Handle navigation events
        if (!ev.isMouseEvent) {
            if (ev.isSpecial) {
                if (ev.keyCode == KeyCode::Up) {  // Up arrow
                    if (m_cursorY > 0) {
                        setCursorPos(m_preferredX, m_cursorY - 1);
                        return true;
                    }
                }
                if (ev.keyCode == KeyCode::Down) {  // Down arrow
                    if (m_cursorY < m_lines.size() - 1) {
                        setCursorPos(m_preferredX, m_cursorY + 1);
                        return true;
                    }
                }
                if (ev.keyCode == KeyCode::PageUp) {
                    int moveAmount = std::min((int)m_cursorY, m_height - 1);
                    m_cursorY -= moveAmount;
                    ensureCursorVisible();
                    return true;
                }
                if (ev.keyCode == KeyCode::PageDown) {
                    int moveAmount = std::min((int)(m_lines.size() - m_cursorY - 1), m_height - 1);
                    m_cursorY += moveAmount;
                    ensureCursorVisible();
                    return true;
                }
                
            } 
        }
        
        // Handle mouse events for selection
        if (ev.isMouseEvent) {
            // Convert mouse coordinates to editor coordinates (1-based to 0-based)
            int relX = (ev.x - 1) - m_x;
            int relY = (ev.y - 1) - m_y;
            
            // Check if mouse is within editor bounds
            if (relX >= 0 && relX < m_width && relY >= 0 && relY < m_height) {
                // Calculate line and character position
                size_t lineIndex = m_topLine + relY;
                
                if (lineIndex < m_lines.size()) {
                    // Convert screen X position to logical character position
                    std::string& line = m_lines[lineIndex];
                    size_t charIndex = screenToLogicalPos(line, relX);
                    
                    if (ev.button == ButtonPressed::Left) {
                        // On mouse press, move cursor and start selection
                        setCursorPos(charIndex, lineIndex);
                        
                        if (!m_isSelecting) {
                            startSelection();
                            m_isSelecting = true;
                        }
                        return true;
                    }
      
                    else if (ev.button == ButtonPressed::Release) {
                        // On mouse release, finalize selection
                        if (m_selection) {
                            setCursorPos(charIndex, lineIndex);
                        }
                        m_isSelecting = false;
                    }
                        
                    //     // If selection start and end are the same, clear it
                    //     if (m_selection->getStart() == m_selection->getEnd()) {
                    //         clearSelection();
                    //     }
                        
                    //     return true;
                    // }
                }
            }
        }
        
        // Handle mouse wheel events
        if (ev.isMouseEvent && (ev.button == ButtonPressed::WheelUp || ev.button == ButtonPressed::WheelDown)) {
            // First, update from EditBox if needed
            // if (m_editBoxChanged) {
            //     updateFromEditBox();
            //     m_editBoxChanged = false;
            // }
            
            // Now handle the scrolling
            if (ev.button == ButtonPressed::WheelUp) {
                // Scroll up by 3 lines
                if (m_topLine > 0) {
                    m_topLine = (m_topLine > 3) ? m_topLine - 3 : 0;
                }
                return true;
            }
            else { // WheelDown
                // Scroll down by 3 lines
                if (m_topLine + m_height < m_lines.size()) {
                    m_topLine = std::min(m_topLine + 3, m_lines.size() - m_height);
                }
                return true;
            }
        }
        
        
        
        return false;
    }
    
    void draw(Terminal& term) override {
        // Update ScrollBar metrics before drawing
        m_scrollBar->setMetrics(m_lines.size(), m_height, m_topLine);
        
        // Draw visible portion of text
        for (int y = 0; y < m_height; y++) {
            size_t lineIndex = m_topLine + y;
            
            // Skip the cursor line - we'll replace it with an EditBox
            if (lineIndex == m_cursorY) {
                continue;
            }
            
            // Clear this line
            for (int x = 0; x < m_width; x++) {
                term.putChar(m_x + x, m_y + y, ExChar(U' ', Default, Default, 0));
            }
            
            // Draw line content if it exists
            if (lineIndex < m_lines.size()) {
                const std::string& line = m_lines[lineIndex];
                int screenX = 0;
                
                // Find logical position for left edge
                size_t startPos = m_leftChar;
                
                // Calculate flat position for the start of this line
                size_t lineStartFlatPos = linePosToFlatPos(lineIndex, 0);
                
                // Draw visible portion of the line
                for (size_t i = startPos; i < line.length() && screenX < m_width; i++) {
                    // Check if this character is selected
                    bool isSelected = m_selection && 
                                     m_selection->contains(lineStartFlatPos + i);
                    
                    Color fg = isSelected ? m_selectionFg : Default;
                    Color bg = isSelected ? m_selectionBg : Default;
                    
                    if (line[i] == '\t') {
                        // Handle tab character
                        int tabWidth = m_tabSize - (screenX % m_tabSize);
                        for (int t = 0; t < tabWidth && screenX < m_width; t++) {
                            term.putChar(m_x + screenX, m_y + y, 
                                        ExChar(U' ', fg, bg, 0));
                            screenX++;
                        }
                    } else {
                        // Regular character
                        term.putChar(m_x + screenX, m_y + y, 
                                    ExChar(line[i], fg, bg, 0));
                        screenX++;
                    }
                }
            }
        }

        // Position the EditBox
        positionEditBox();
        
        // Draw the EditBox only if the cursor line is visible
        if (m_cursorY >= m_topLine && m_cursorY < m_topLine + m_height) {
            m_editBox->draw(term);
        }
        
        // Draw the ScrollBar
        m_scrollBar->draw(term);
        
        // Draw the status bar if it exists
        if (m_statusBar) {
            m_statusBar->draw(term);
        }
    }
    
    // Set the text content
    void setText(const std::string& text) {
        m_lines.clear();
        
        // Split text into lines
        size_t start = 0;
        size_t end = text.find('\n');
        
        while (end != std::string::npos) {
            m_lines.push_back(text.substr(start, end - start));
            start = end + 1;
            end = text.find('\n', start);
        }
        
        // Add the last line
        m_lines.push_back(text.substr(start));
        
        // Reset cursor and scroll position
        m_cursorX = m_cursorY = 0;
        m_leftChar = m_topLine = 0;
    }
    
    // Get the text content
    std::string getText() const {
        std::string result;
        for (size_t i = 0; i < m_lines.size(); i++) {
            result += m_lines[i];
            if (i < m_lines.size() - 1) {
                result += '\n';
            }
        }
        return result;
    }
    
    // Getters and setters
    size_t getCursorX() const { return m_cursorX; }
    size_t getCursorY() const { return m_cursorY; }
    
    void setCursorPos(size_t x, size_t y, bool ensureCurVisible = true) {
        if (x == m_cursorX && y == m_cursorY) return;

        if (m_cursorY != y) {
            updateFromEditBox();
            m_editBoxChanged = false;
        }
        m_cursorX = x;
        m_cursorY = y;
        
        // Clamp cursor position
        if (m_cursorY >= m_lines.size()) {
            m_cursorY = m_lines.size() - 1;
        }
        
        if (m_cursorY < m_lines.size() && m_cursorX > m_lines[m_cursorY].length()) {
            m_cursorX = m_lines[m_cursorY].length();
        }

        // Update preferred X for any horizontal movement or when cursor changes due to mouse/editing
        if (m_cursorY == y) {  // Only update when moving horizontally or editing
            m_preferredX = m_cursorX;
        }
        
        // Handle selection
        if (m_isSelecting) {
            updateSelection();
        } 
        else {
            clearSelection();
        }
        
        if (ensureCurVisible) ensureCursorVisible();
        
        updateCursorInfo();

    }
    
    void setWheelScrollLines(int lines) { m_numLinesWheelScroll = lines; }
    int getWheelScrollLines() const { return m_numLinesWheelScroll; }

    int getTabSize() const { return m_tabSize; }
    void setTabSize(int tabSize) { m_tabSize = tabSize; }
    
    // Get the status bar (if any)
    StatusBar* getStatusBar() {
        return m_statusBar;
    }

    // Set how often to update from EditBox (in number of events)
    void setUpdateFrequency(int frequency) { m_updateFrequency = frequency; }
    int getUpdateFrequency() const { return m_updateFrequency; }

    // Selection methods
    bool hasSelection() const { return m_selection != nullptr; }
    
    void startSelection() {
        // Start selection at current cursor position
        size_t flatPos = linePosToFlatPos(m_cursorY, m_cursorX);
        
        // Clear any existing selection
        clearSelection();
        
        // Create new selection
        m_selection = new RangeSelection(flatPos, flatPos);
    }
    
    void updateSelection() {
        if (m_selection) {
            // Update end of selection to current cursor position
            size_t flatPos = linePosToFlatPos(m_cursorY, m_cursorX);
            size_t start = m_selection->getStart();
            
            ((RangeSelection*)m_selection)->update(start, flatPos);

        }
    }
    
    void clearSelection() {
        if (m_selection) {
            delete m_selection;
            m_selection = nullptr;
        }
    }
    
    // Get selected text
    std::string getSelectedText() const {
        if (!m_selection) {
            return "";
        }
        
        int start = m_selection->getStart();
        int end = m_selection->getEnd();
        if (start > end)  std::swap(start, end);
        
        return getTextAt(start, end - start);
    }
    
    // Delete selected text
    void deleteSelection() {
        if (m_selection) {
            ((RangeSelection*)m_selection)->fix();
            size_t start  = m_selection->getStart();
            size_t length = m_selection->getEnd() - start;
            
            // Delete the text
            deleteText(start, length);
          
            // Move cursor to the start of the selection
            size_t line, linePos;
            flatPosToLinePos(start, line, linePos);
            setCursorPos(linePos, line);
            
            

            // Clear the selection
            clearSelection();
        }
    }
    
    // Set selection colors
    void setSelectionColors(Color fg, Color bg) {
        m_selectionFg = fg;
        m_selectionBg = bg;
    }

    void setSelection(Selection* selection) {
        // Clear any existing selection
        clearSelection();
        
        // Set the new selection
        m_selection = selection;
    }

    // Called by EditBox when its cursor position changes
    void editBoxCursorChanged(EditBox* editBox, size_t newCursorPos) {
        if (editBox == m_editBox && m_cursorY < m_lines.size()) {
            // Update the Editor's cursor X position to match the EditBox
            m_cursorX = newCursorPos;
            
            // Ensure cursor is visible
            ensureCursorVisible();
            
            // Update cursor info in status bar if present
            updateCursorInfo();
        }
    }

    // Friend declaration to allow EditBox to call private methods
    friend class EditBox;
    
    // Method for EditBox to call when at boundaries
    bool handleEditBoxBoundary(bool isAtStart, bool isDelete);

    // Set the file name
    void setFileName(const std::string& fileName) {
        m_fileName = fileName;
    }
    
    // Get the file name
    const std::string& getFileName() const {
        return m_fileName;
    }
    
    // Load content from a file
    bool loadFromFile(const std::string& fileName) {
        std::ifstream file(fileName);
        if (!file.is_open()) {
            return false;
        }
        
        // Clear existing content
        m_lines.clear();
        
        // Read file line by line
        std::string line;
        while (std::getline(file, line)) {
            m_lines.push_back(line);
        }
        
        // If file is empty, add at least one empty line
        if (m_lines.empty()) {
            m_lines.push_back("");
        }
        
        // Set the file name
        m_fileName = fileName;
        
        // Reset cursor position
        m_cursorX = 0;
        m_cursorY = 0;
        
        // Update EditBox if present
        if (m_editBox) {
            positionEditBox();
            updateEditBoxFromCurrentLine();
        }
        
        return true;
    }

    // Save content to a file
    bool saveToFile(const std::string& fileName) {
        std::ofstream file(fileName);
        if (!file.is_open()) {
            return false;
        }
        
        // Write all lines to the file
        for (size_t i = 0; i < m_lines.size(); ++i) {
            file << m_lines[i];
            // Add newline after each line except the last one
            if (i < m_lines.size() - 1) {
                file << '\n';
            }
        }
        
        // Set the file name
        m_fileName = fileName;
        
        return true;
    }
    
    // Save to the current file name
    bool save() {
        if (m_fileName.empty()) {
            return false;  // No file name set
        }
        return saveToFile(m_fileName);
    }
};

// Base class for controls that support text editing with undo/redo

// Enum for diff operation types
enum class DiffOpType {
    NONE,       // No difference
    INSERT,     // Text was inserted
    DELETE,     // Text was deleted
    REPLACE     // Text was replaced
};

// Structure to hold diff operation details
struct DiffOp {
    DiffOpType type;
    size_t position;     // Position in the original text
    std::string oldText; // Text to be deleted/replaced (empty for INSERT)
    std::string newText; // Text to be inserted/replaced (empty for DELETE)
    
    DiffOp() : type(DiffOpType::NONE), position(0) {}
};



} // namespace term 