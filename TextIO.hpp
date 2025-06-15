// TextIO.hpp
#pragma once
#include <string>

// A namespace for all text input/output related functions
namespace TextIO {
    void print(const std::string& message);
    void print_uw(uint16_t value);
    void print_uwhex(uint16_t value);
    void nl(); // Newline
    void clearScreen();
    void setColor(uint8_t foreground, uint8_t background);
    void locate(int row, int col);
    void setCursor(bool on);
}