#pragma once
#include <vector>
#include <string>
#include <regex> 
#include <windows.h>

class TextEditor {
public:
    TextEditor(std::vector<std::string>& lines);
    void run();

private:
    void process_keypress(int c);
    void draw_screen();
    void draw_status_bar();
    void debug(std::string msg);
    void move_cursor(int key);
    void draw_line(const std::string& line);
    void get_window_size(int& rows, int& cols);

    std::vector<std::string>& lines_ref;
    int cx = 0, cy = 0; // Cursor x and y position within the text buffer
    int screen_cols;
    int screen_rows;
    int top_row = 0; // The top row of the file being displayed
    bool is_debug = false;
    std::string status_msg;

    std::regex keyword_regex;
    // Define some colors for readability
    const int COLOR_DEFAULT = 15; // White
    const int COLOR_KEYWORD = 12; // Bright Blue
    const int COLOR_STRING = 10;  // Bright Green
    const int COLOR_COMMENT = 7;  // Gray
    const int COLOR_NUMBER = 13;  // Bright Magenta
};