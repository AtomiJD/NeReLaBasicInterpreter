#include "TextEditor.hpp"
#include "TextIO.hpp"
#include <conio.h>
#include <algorithm>

#define CTRL_KEY(k) ((k) & 0x1f)

void TextEditor::get_window_size(int& rows, int& cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    else { // Fallback to default size on error
        cols = 80;
        rows = 25;
    }
}

TextEditor::TextEditor(std::vector<std::string>& lines) : lines_ref(lines) {
    get_window_size(screen_rows, screen_cols);
    screen_rows -= 3; // Leave space for status bar and prompt
    if (lines_ref.empty()) {
        lines_ref.push_back(""); // Start with one empty line if the file is new
    }
    std::string keyword_pattern = "\\b(PRINT|IF|THEN|ELSE|ENDIF|FOR|TO|NEXT|STEP|GOTO|FUNC|ENDFUNC|SUB|ENDSUB|RETURN|STOP|RESUME|DIM|AS|INTEGER|STRING|DOUBLE|DATE|LEFT\\$|RIGHT\\$|MID\\$|LEN|ASC|CHR\\$|INSTR|LCASE\\$|UCASE\\$|TRIM\\$|INKEY\\$|VAL|STR\\$|SIN|COS|TAN|SQR|RND|TICK|NOW|DATE\\$|TIME\\$|DATEADD|CVDATE|CLS|LOCATE|SLEEP|CURSOR|DIR|CD|PWD|COLOR|MKDIR|KILL)\\b";
    keyword_regex = std::regex(keyword_pattern, std::regex::icase);
}

// In TextEditor.cpp, add this new function

// In TextEditor.cpp, replace the old draw_line function

void TextEditor::draw_line(const std::string& line) {
    size_t pos = 0;
    std::string line_upper = line;
    std::transform(line_upper.begin(), line_upper.end(), line_upper.begin(), ::toupper);

    while (pos < line.length()) {
        // --- Precedence 1: Comments ---
        // An apostrophe or a REM keyword starts a comment.
        size_t comment_start_apos = line.find('\'', pos);
        size_t rem_pos = std::string::npos;
        if (line_upper.rfind("REM ", pos) == pos) {
            rem_pos = pos;
        }

        if (comment_start_apos != std::string::npos || rem_pos != std::string::npos) {
            size_t comment_start = (comment_start_apos < rem_pos) ? comment_start_apos : rem_pos;
            TextIO::setColor(COLOR_DEFAULT, 0);
            TextIO::print(line.substr(pos, comment_start - pos));
            TextIO::setColor(COLOR_COMMENT, 0);
            TextIO::print(line.substr(comment_start));
            return; // The rest of the line is a comment
        }

        // --- Precedence 2: Strings ---
        if (line[pos] == '"') {
            size_t string_end = line.find('"', pos + 1);
            if (string_end == std::string::npos) string_end = line.length();
            else string_end++; // Include the closing quote

            TextIO::setColor(COLOR_STRING, 0);
            TextIO::print(line.substr(pos, string_end - pos));
            pos = string_end;
            continue;
        }

        // --- Precedence 3: Numbers ---
        if (isdigit(line[pos])) {
            size_t num_end = pos;
            while (num_end < line.length() && (isdigit(line[num_end]) || line[num_end] == '.')) {
                num_end++;
            }
            TextIO::setColor(COLOR_NUMBER, 0);
            TextIO::print(line.substr(pos, num_end - pos));
            pos = num_end;
            continue;
        }

        // --- Precedence 4: Keywords and default text ---
        std::string search_area = line.substr(pos);
        std::smatch match;
        if (std::regex_search(search_area, match, keyword_regex) && match.prefix().length() == 0) {
            // A keyword is at the current position
            TextIO::setColor(COLOR_KEYWORD, 0);
            TextIO::print(match[0].str());
            pos += match[0].length();
            continue;
        }

        // --- Fallback: Print one character in default color ---
        TextIO::setColor(COLOR_DEFAULT, 0);
        TextIO::print(std::string(1, line[pos]));
        pos++;
    }
}
void TextEditor::draw_status_bar() {
    TextIO::setCursor(false);
    TextIO::locate(screen_rows + 1, 1);
    std::string status = " jdBasic Editor | Line: " + std::to_string(cy + 1) + " | ^S: Save | ^X: Exit ";
    if (status.length() < (size_t)screen_cols) {
        status += std::string(screen_cols - status.length(), ' ');
    }
    TextIO::print(status);

    TextIO::locate(screen_rows + 2, 1);
    if (status_msg.length() < (size_t)screen_cols) {
        status_msg += std::string(screen_cols - status_msg.length(), ' ');
    }
    TextIO::print(status_msg);
    status_msg.clear();
    TextIO::setCursor(true);
}

void TextEditor::debug(std::string msg) {
    if (is_debug)
        status_msg = " Debug: " + msg;
    else
        status_msg.clear();
}

void TextEditor::draw_screen() {
    TextIO::setColor(COLOR_DEFAULT, 0); // Set default color
    TextIO::clearScreen();
    for (int y = 0; y < screen_rows - 1; ++y) {
        int file_row = top_row + y;
        TextIO::locate(y + 1, 1);
        if (file_row < lines_ref.size()) {
            draw_line(lines_ref[file_row]); // Use our new highlighting function
        }
        else {
            TextIO::setColor(8, 0); // Gray for tilde
            TextIO::print("~");
        }
    }
    TextIO::setColor(COLOR_DEFAULT, 0); // Reset color for status bar
    draw_status_bar();
}

void TextEditor::move_cursor(int key) {
    debug("S: " + std::to_string(key));
    switch (key) {
    case 72: // Arrow Up
        if (cy > 0) cy--;
        break;
    case 80: // Arrow Down
        if (cy < lines_ref.size() - 1) cy++;
        break;
    case 75: // Arrow Left
        if (cx > 0) cx--;
        break;
    case 77: // Arrow Right
        if (cy < lines_ref.size() && cx < lines_ref[cy].length()) cx++;
        break;
    case 73: // Page Up
        cy = (cy < screen_rows) ? 0 : cy - (screen_rows - 1);
        break;
    case 81: // Page Down
        cy += (screen_rows - 1);
        if (cy >= lines_ref.size()) cy = lines_ref.size() - 1;
        break;
    }

    // --- SCROLLING LOGIC ---
    if (cy < top_row) {
        top_row = cy;
    }
    if (cy >= top_row + screen_rows) {
        top_row = cy - screen_rows + 1;
    }

    // Snap cursor to end of line if it's past the end
    if (cy < lines_ref.size() && cx > lines_ref[cy].length()) {
        cx = lines_ref[cy].length();
    }
}

void TextEditor::process_keypress(int c) { // Note the new 'int c' parameter
    if (c == 224 || c == 0) {
        int key = _getch();
        if (key == 83) { // Delete Key
            if (cx < lines_ref[cy].length()) {
                lines_ref[cy].erase(cx, 1);
            }
            else if (cy < lines_ref.size() - 1) {
                // Join with next line
                lines_ref[cy] += lines_ref[cy + 1];
                lines_ref.erase(lines_ref.begin() + cy + 1);
            }
        }
        else {
            move_cursor(key);
        }
        return;
    }
    debug("N: " + std::to_string(c));
    switch (c) {
        // Ctrl key commands are handled by the caller (run loop)
    case CTRL_KEY('x'):
    case CTRL_KEY('s'):
    case CTRL_KEY('d'):
        break;

    case 13: // Enter key
        if (cx == lines_ref[cy].length()) {
            lines_ref.insert(lines_ref.begin() + cy + 1, "");
        }
        else {
            std::string line_end = lines_ref[cy].substr(cx);
            lines_ref[cy].erase(cx);
            lines_ref.insert(lines_ref.begin() + cy + 1, line_end);
        }
        cy++;
        cx = 0;
        break;

    case 8: // Backspace
        if (cx > 0) {
            lines_ref[cy].erase(cx - 1, 1);
            cx--;
        }
        else if (cy > 0) {
            cx = lines_ref[cy - 1].length();
            lines_ref[cy - 1] += lines_ref[cy];
            lines_ref.erase(lines_ref.begin() + cy);
            cy--;
        }
        break;

    default: // Insert any other character
        if (isprint(c)) { // Only print printable characters
            lines_ref[cy].insert(cx, 1, (char)c);
            cx++;
        }
        break;
    }
}


void TextEditor::run() {
    int key;
    TextIO::setCursor(true);
    while (true) {
        draw_screen();
        // Position the real cursor
        TextIO::locate(cy - top_row + 1, cx + 1);

        // Read the keypress in ONE place
        key = _getch();

        // Handle commands first
        if (key == CTRL_KEY('x')) {
            TextIO::locate(screen_rows + 1, 1);
            break; // Exit loop
        }
        else if (key == CTRL_KEY('s')) {
            status_msg = "Save command issued! (Save on exit)";
            continue; // Redraw screen with new status message
        }
        else if (key == CTRL_KEY('d')) {
            is_debug = !is_debug;
            continue; 
        }

        // Process all other keys
        process_keypress(key);
    }
    TextIO::setColor(2, 0); // Reset color on exit
    TextIO::setCursor(true);
}