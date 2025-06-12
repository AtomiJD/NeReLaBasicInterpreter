// Error.cpp
#include "Error.hpp"
#include "TextIO.hpp" // We need this to print the error messages.
#include <vector>

namespace {
    // This variable holds the current error code.
    // It's in an anonymous namespace, making it accessible only within this file.
    uint8_t current_error_code = 0;
    uint16_t error_line_number = 0;

    // A table of error messages. We can expand this as we go.
    // Using a vector of strings makes it easy to manage.
    const std::vector<std::string> errorMessages = {
        "OK",                  // 0
        "Syntax Error",        // 1
        "Calculation Error",   // 2
        "Variable not found",  // 3
        "Unclosed IF/ENDIF",   // 4
        "Unclosed FUNC/ENDFUNC",// 5
        "File not found",       // 6
        "Function/Sub name not found", //7
        "Wrong number of arguments", //8
        "RETURN without GOSUB/CALL", //9
        "Array out of bounds" //10
        // Add more messages as needed
    };
}

void Error::set(uint8_t errorCode, uint16_t lineNumber) {
    if (current_error_code == 0) { // Only store the first error
        current_error_code = errorCode;
        error_line_number = lineNumber;
    }
}

uint8_t Error::get() {
    return current_error_code;
}

void Error::clear() {
    current_error_code = 0;
    error_line_number = 0;
}

std::string Error::getMessage(uint8_t errorCode) {
    if (errorCode < errorMessages.size()) {
        return errorMessages[errorCode];
    }
    return "Unknown Error";
}

void Error::print() {
    if (current_error_code != 0) {
        TextIO::print("? Error #" + std::to_string(current_error_code) + "," + getMessage(current_error_code));
        if (error_line_number > 0) {
            TextIO::print(" IN LINE " + std::to_string(error_line_number));
        }
        TextIO::nl();
    }
}