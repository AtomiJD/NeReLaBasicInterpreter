// TextIO.cpp
#include "TextIO.hpp"
#include <iostream>
#include <iomanip> // For std::hex

void TextIO::print(const std::string& message) {
    std::cout << message;
}

void TextIO::print_uw(uint16_t value) {
    // Explicitly set the stream to decimal mode before printing.
    std::cout << std::dec << value;
}

void TextIO::print_uwhex(uint16_t value) {
    // std::hex makes the output hexadecimal
    // std::setw and std::setfill ensure it's padded with zeros to 4 digits
    std::cout << '$' << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << value;
}

void TextIO::nl() {
    std::cout << '\n';
}

void TextIO::clearScreen() {
    // This is OS-dependent. For now, a simple simulation.
    // On Windows, you could use: system("cls");
    std::cout << "\n--- CLS ---\n\n";
}

void TextIO::setColor(uint8_t foreground, uint8_t background) {
    // We can implement actual color later using the Windows Console API
    // For now, we'll just print a notification.
    std::cout << "[Color set to FG:" << (int)foreground << ", BG:" << (int)background << "]\n";
}