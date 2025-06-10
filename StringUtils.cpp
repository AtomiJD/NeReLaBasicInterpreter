// StringUtils.cpp
#include "StringUtils.hpp"
#include <cctype>   // Required for isspace, isalpha, isdigit
#include <algorithm>// Required for std::find_if

// A helper function to check if a character is NOT a whitespace.
// We'll use this with the strip function.
bool is_not_space(char c) {
    return !std::isspace(static_cast<unsigned char>(c));
}

bool StringUtils::isspace(char c) {
    // The cast is important for handling different character sets correctly.
    return std::isspace(static_cast<unsigned char>(c));
}

bool StringUtils::isletter(char c) {
    return std::isalpha(static_cast<unsigned char>(c));
}

bool StringUtils::isdigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool StringUtils::contains(const std::string& str, char c) {
    // string::find returns string::npos if the character isn't found.
    return str.find(c) != std::string::npos;
}

void StringUtils::strip(std::string& str) {
    // Erase leading whitespace
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), is_not_space));

    // Erase trailing whitespace
    str.erase(std::find_if(str.rbegin(), str.rend(), is_not_space).base(), str.end());
}