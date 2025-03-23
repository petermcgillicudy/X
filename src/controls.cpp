#include "controls.hpp"
// #include "misc.hpp"

namespace term {

// Function to compute the difference between two strings
DiffOp computeDiff(const std::string& oldText, const std::string& newText) {
    DiffOp result;
    
    // If texts are identical, return NONE operation
    if (oldText == newText) {
        return result;
    }
    
    // Find common prefix
    size_t prefixLen = 0;
    while (prefixLen < oldText.length() && prefixLen < newText.length() && 
           oldText[prefixLen] == newText[prefixLen]) {
        prefixLen++;
    }
    
    // Find common suffix
    size_t oldSuffixStart = oldText.length();
    size_t newSuffixStart = newText.length();
    
    while (oldSuffixStart > prefixLen && newSuffixStart > prefixLen && 
           oldText[oldSuffixStart-1] == newText[newSuffixStart-1]) {
        oldSuffixStart--;
        newSuffixStart--;
    }
    
    // Set the position where the change starts
    result.position = prefixLen;
    
    // Determine the type of operation
    if (prefixLen == oldSuffixStart) {
        // No text was deleted, only inserted
        result.type = DiffOpType::INSERT;
        result.newText = newText.substr(prefixLen, newSuffixStart - prefixLen);
    }
    else if (prefixLen == newSuffixStart) {
        // No text was inserted, only deleted
        result.type = DiffOpType::DELETE;
        result.oldText = oldText.substr(prefixLen, oldSuffixStart - prefixLen);
    }
    else {
        // Some text was replaced
        result.type = DiffOpType::REPLACE;
        result.oldText = oldText.substr(prefixLen, oldSuffixStart - prefixLen);
        result.newText = newText.substr(prefixLen, newSuffixStart - prefixLen);
    }
    
    return result;
}



void EditBox::setCursorPos(size_t newPos) {
    // Clamp position to valid range
    if (newPos > m_text.length()) {
        newPos = m_text.length();
    }
    
    m_cursorPos = newPos;
    
    // Adjust scroll position if needed
    if (m_cursorPos < m_leftIndex) {
        // Cursor moved left of visible area
        m_leftIndex = m_cursorPos;
    } else if (m_cursorPos >= m_leftIndex + m_width) {
        // Cursor moved right of visible area
        m_leftIndex = m_cursorPos - m_width + 1;
    }
}

bool EditBox::processEvent(const SGREvent& ev) {
    if (!m_hasFocus) return false;
    
    if (!ev.isMouseEvent) {
        if (ev.isSpecial) {
            // Handle special keys
            if (ev.keyCode == KeyCode::Left) {  // Left arrow
                if (m_cursorPos > 0) {
                    
                    setCursorPos(m_cursorPos - 1);
                } else {
                    // At beginning of line
                    return handleBoundary(true, false);
                }
                return true;
            }
            if (ev.keyCode == KeyCode::Right) {  // Right arrow
                if (m_cursorPos < m_text.length()) {
                    setCursorPos(m_cursorPos + 1);
                } else {
                    // At end of line
                    return handleBoundary(false, false);
                }
                return true;
            }
            if (ev.keyCode == KeyCode::Delete) {  // Delete
                if (m_cursorPos < m_text.length()) {
                    // Normal delete within the line
                    deleteText(m_cursorPos, 1);
                } else {
                    // At end of line
                    return handleBoundary(false, true);
                }
                return true;
            }
            if (ev.keyCode == KeyCode::Left && ev.ctrl) {  // Ctrl+Left
                setCursorPos(findPrevWordStart(m_text, m_cursorPos));
                return true;
            }
            if (ev.keyCode == KeyCode::Right && ev.ctrl) {  // Ctrl+Right
                setCursorPos(findNextWordEnd(m_text, m_cursorPos));
                return true;
            }
            if (ev.keyCode == KeyCode::Delete && ev.ctrl) {  // Ctrl+Delete - delete to end of word
                deleteParentSelectedText();
                if (m_cursorPos < m_text.length()) {
                    size_t wordEnd = findNextWordEnd(m_text, m_cursorPos);
                    if (wordEnd > m_cursorPos) {
                        // Replace direct text modification with deleteText
                        deleteText(m_cursorPos, wordEnd - m_cursorPos);
                        return true;
                    }
                }
            }            
            if (ev.keyCode == KeyCode::Home && !ev.ctrl) {  // Home
                setCursorPos(0);
                return true;
            }
            if (ev.keyCode == KeyCode::End && !ev.ctrl) {  // End
                setCursorPos(m_text.length());
                return true;
            }
            if (ev.keyCode == KeyCode::Insert) {  // Insert
                toggleInsertMode();
            }
        } else {
            // Handle regular keys
            
            if (ev.keyCode == KeyCode::Enter) {  // Enter key
                // Delete any selected text first
                deleteParentSelectedText();
                
                // If we have a parent editor, let it handle the newline
                if (m_parentEditor) {
                                        // Calculate the flat position for the cursor
                    size_t flatPos = m_parentEditor->linePosToFlatPos(m_parentEditor->getCursorY(), m_cursorPos);
                    
                    // Insert a newline character at the cursor position using the undoable action
                    m_parentEditor->insertText(flatPos, "\n");
                    
                    // The cursor position will be updated in the Editor
                    // but we need to update the EditBox text manually
                    m_text = m_parentEditor->m_cursorY < m_parentEditor->m_lines.size() ? 
                             m_parentEditor->m_lines[m_parentEditor->m_cursorY] : "";
                    m_cursorPos = m_parentEditor->m_cursorX;
                    
                    // The EditBox will be repositioned in the next draw cycle
                    return true;
                } 
            }
            
            // Check for Ctrl+Z and Ctrl+Y
            if (ev.key == 'z' && ev.ctrl) {  // Ctrl+Z for undo
                undo();
                return true;
            }
            else if (ev.key == 'y' && ev.ctrl) {  // Ctrl+Y for redo
                redo();
                return true;
            }
            if (ev.keyCode == KeyCode::Backspace) {  // Backspace
                // Delete selection if exists
                if (deleteParentSelectedText()) {
                    return true;
                }
                
                if (m_cursorPos > 0) {
                    // Normal backspace within the line
                    deleteText(m_cursorPos - 1, 1);
                } else {
                    // At beginning of line
                    return handleBoundary(true, true);
                }
                return true;
            }
            else if (ev.keyCode == KeyCode::Tab) {  // Tab
                deleteParentSelectedText();
                // Replace direct text modification with insertText
                insertText(m_cursorPos, std::string(m_tabSize, ' '));
                return true;
            }
            else if (ev.key >= 32) {  // Printable characters
                deleteParentSelectedText();
                if (m_insertMode) {
                    // Replace direct text modification with insertText
                    insertText(m_cursorPos, std::string(1, ev.key));
                } else {
                    if (m_cursorPos < m_text.length()) {
                        // Replace with deleteText followed by insertText
                        deleteText(m_cursorPos, 1);
                    }
                    insertText(m_cursorPos, std::string(1, ev.key));
                }
                return true;
            }
        }
    }
    return false;
}

void EditBox::draw(Terminal& term) {
    // Draw a box around the edit area
    for (int i = 0; i < m_width; i++) {
        term.putChar(m_x + i, m_y, ExChar(U' ', Default, m_backgroundColor, 0));
    }
    
    // Check if we have a parent editor with a selection
    bool hasSelection = m_parentEditor && m_parentEditor->hasSelection();
    
    // Calculate the flat position of the first character in this line
    size_t lineStartFlatPos = 0;
    if (hasSelection && m_parentEditor) {
        lineStartFlatPos = m_parentEditor->linePosToFlatPos(m_parentEditor->m_cursorY, 0);
    }
    
    // Draw text with proper tab handling and selection highlighting
    int screenX = 0;
    for (size_t i = m_leftIndex; i < m_text.length() && screenX < m_width; i++) {
        // Check if this character is within the selection
        bool isSelected = false;
        if (hasSelection) {
            // Calculate the flat position of this character
            size_t charFlatPos = lineStartFlatPos + i;
            isSelected = m_parentEditor->m_selection->contains(charFlatPos);
        }
        
        Color selectionFg = m_parentEditor->getSelectionFg();
        Color selectionBg = m_parentEditor->getSelectionBg();
        
        if (m_text[i] == '\t') {
            // Handle tab character
            int tabWidth = m_tabSize - (screenX % m_tabSize);
            for (int t = 0; t < tabWidth && screenX < m_width; t++) {
                // Draw with selection highlighting if needed
                if (isSelected) {
                    term.putChar(m_x + screenX, m_y, ExChar(U' ', selectionFg, selectionBg, 0));
                } else {
                    term.putChar(m_x + screenX, m_y, ExChar(U' ', Default, m_backgroundColor, 0));
                }
                screenX++;
            }
        } else {
            // Regular character - draw with selection highlighting if needed
            if (isSelected) {
                term.putChar(m_x + screenX, m_y, ExChar(m_text[i], selectionFg, selectionBg, 0));
            } else {
                term.putChar(m_x + screenX, m_y, ExChar(m_text[i], Default, m_backgroundColor, 0));
            }
            screenX++;
        }
    }
    
    // Draw cursor if we have focus
    if (m_hasFocus) {
        // Calculate cursor screen position
        int cursorScreenX = 0;
        for (size_t i = m_leftIndex; i < m_cursorPos; i++) {
            if (m_text[i] == '\t') {
                cursorScreenX += m_tabSize - (cursorScreenX % m_tabSize);
            } else {
                cursorScreenX++;
            }
        }
        
        if (cursorScreenX >= 0 && cursorScreenX < m_width) {
            // Draw cursor
            char cursorChar = ' ';
            if (m_cursorPos < m_text.length()) {
                cursorChar = m_text[m_cursorPos];
                if (cursorChar == '\t') cursorChar = ' ';
            }
            
            term.putChar(m_x + cursorScreenX, m_y, ExChar(cursorChar, Black, Yellow, STYLE_BOLD));
        }
    }
}

void Editor::updateFromEditBox() {
    
    if (m_editBox && m_cursorY < m_lines.size()) {
        std::string editBoxText = m_editBox->getText();
        std::string currentLine = m_lines[m_cursorY];
        
        // Compute the difference
        DiffOp diff = computeDiff(currentLine, editBoxText);
        
        // Apply the change if there is any
        switch (diff.type) {
            case DiffOpType::INSERT:
                insertText(linePosToFlatPos(m_cursorY, diff.position), diff.newText);
                break;
                
            case DiffOpType::DELETE:
                deleteText(linePosToFlatPos(m_cursorY, diff.position), diff.oldText.length());
                break;
                
            case DiffOpType::REPLACE:
                replaceText(linePosToFlatPos(m_cursorY, diff.position), 
                           diff.oldText.length(), diff.newText);
                break;
                
            case DiffOpType::NONE:
                // No change needed
                break;
        }
        
        // Update cursor position if there was a change
        if (diff.type != DiffOpType::NONE) {
            m_cursorX = m_editBox->getCursorPos();
        }
    }
}

void Editor::updateEditBoxFromCurrentLine() {
    if (m_editBox && m_cursorY < m_lines.size()) {
        m_editBox->setText(m_lines[m_cursorY]);
        m_editBox->setCursorPos(m_cursorX);
        
        // Reset the changed flag since we just synced
        m_editBoxChanged = false;
    }
}


bool EditBox::deleteParentSelectedText() {
    if (m_parentEditor && m_parentEditor->hasSelection()) {
        // Get the selection range in flat coordinates
        size_t startPos = m_parentEditor->m_selection->getStart();
        size_t endPos = m_parentEditor->m_selection->getEnd();
        
        // Make sure start <= end
        if (startPos > endPos) {
            std::swap(startPos, endPos);
        }
        
        // Delete the selected text (using existing method that supports undo)
        m_parentEditor->deleteText(startPos, endPos - startPos);
        
        // Clear the selection
        m_parentEditor->clearSelection();
        
        // Synchronize with editor
        m_text = m_parentEditor->m_cursorY < m_parentEditor->m_lines.size() ? 
                 m_parentEditor->m_lines[m_parentEditor->m_cursorY] : "";
        m_cursorPos = m_parentEditor->m_cursorX;
        
        return true;
    }
    return false;
}

bool EditBox::handleBoundary(bool isAtStart, bool isDelete) {
    // If we have a parent editor, delegate to it
    if (m_parentEditor) {
        return m_parentEditor->handleEditBoxBoundary(isAtStart, isDelete);
    }
    return false;
}

bool Editor::handleEditBoxBoundary(bool isAtStart, bool isDelete) {
    if (isDelete) {
        if (!isAtStart) {
            // Delete at end of line - join with next line
            if (m_cursorY < m_lines.size() - 1) {
                // Store current position
                updateFromEditBox();
                size_t joinPos = m_lines[m_cursorY].length();
                
                // Join current line with next line
                std::string nextLine = m_lines[m_cursorY + 1];
                
                // Create a replace command that will handle the join operation
                size_t currentPos = linePosToFlatPos(m_cursorY, joinPos);
                replaceText(currentPos, 1, ""); // Delete the newline
                
                // Update cursor position
                updateEditBoxFromCurrentLine();
                setCursorPos(joinPos, m_cursorY);
                
                // Update EditBox
                updateEditBoxFromCurrentLine();
                
                return true;
            }
        } else {
            // Backspace at start of line - join with previous line
            if (m_cursorY > 0) {
                updateFromEditBox();
                
                // Calculate join position (end of previous line)
                const int cursorY = m_cursorY;
                size_t joinPos = m_lines[cursorY - 1].length();
                
                // Create a replace command
                size_t prevLineEnd = linePosToFlatPos(cursorY - 1, joinPos);
                replaceText(prevLineEnd, 1, ""); // Delete the newline
                
                // Move cursor to previous line at join position
                updateEditBoxFromCurrentLine();
                setCursorPos(joinPos, cursorY - 1);
                
                // Update EditBox
                positionEditBox();
                // updateEditBoxFromCurrentLine();
                
                return true;
            }
        }
    } else {
        // Navigation (not deletion)
        if (isAtStart) {
            // Left at start of line
            if (m_cursorY > 0) {
                m_cursorY--;
                m_cursorX = m_lines[m_cursorY].length();
                ensureCursorVisible();
                updateCursorInfo();
                return true;
            }
        } else {
            // Right at end of line
            if (m_cursorY < m_lines.size() - 1) {
                m_cursorY++;
                m_cursorX = 0;
                ensureCursorVisible();
                updateCursorInfo();
                return true;
            }
        }
    }
    
    return false;  
}

} // namespace term 