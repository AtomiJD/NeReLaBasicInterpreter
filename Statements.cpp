// Statements.cpp
#include "Statements.hpp"
#include <string>
#include <unordered_map>
#include <cctype> // For toupper
#include <algorithm>

namespace {
    // Helper function to convert a string to uppercase for case-insensitive matching.
    std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::toupper(c); }
        );
        return s;
    }

    // The keyword lookup table. Using std::unordered_map is very efficient.
    const std::unordered_map<std::string, Tokens::ID> keyword_map = {
        {"LIST",    Tokens::ID::LIST},
        {"RUN",     Tokens::ID::RUN},
        {"EDIT",    Tokens::ID::EDIT},
        {"DIR",     Tokens::ID::DIR},
        {"SAVE",     Tokens::ID::SAVE},
        {"LOAD",    Tokens::ID::LOAD},
        {"CLS",     Tokens::ID::CLS},
        {"FUNC",    Tokens::ID::FUNC},
        {"ENDFUNC", Tokens::ID::ENDFUNC},
        {"FOR",     Tokens::ID::FOR},
        {"IF",      Tokens::ID::IF},
        {"THEN",    Tokens::ID::THEN},
        {"ELSE",    Tokens::ID::ELSE},
        {"ENDIF",   Tokens::ID::ENDIF},
        {"AND",     Tokens::ID::AND},
        {"OR",      Tokens::ID::OR},
        {"NOT",     Tokens::ID::NOT},
        {"MOD",     Tokens::ID::MOD},
        {"TRUE",    Tokens::ID::TRUE},
        {"FALSE",   Tokens::ID::FALSE},
        {"GOTO",    Tokens::ID::GOTO},
        {"PRINT",   Tokens::ID::PRINT},
        {"RETURN",  Tokens::ID::RETURN},
        {"TO",      Tokens::ID::TO},
        {"NEXT",    Tokens::ID::NEXT},
        {"STEP",    Tokens::ID::STEP},
        {"PEEK",    Tokens::ID::PEEK},
        {"POKE",    Tokens::ID::POKE},
        {"INPUT",   Tokens::ID::INPUT},
        {"TRON",    Tokens::ID::TRON},
        {"TROFF",   Tokens::ID::TROFF},
        // --- From your cx16_table ---
        {"VPEEK",   Tokens::ID::VPEEK},
        {"VPOKE",   Tokens::ID::VPOKE},
        {"COLOR",   Tokens::ID::COLOR},
        {"LOCATE",  Tokens::ID::LOCATE},
        {"LINE",    Tokens::ID::LINE},
        {"RECT",    Tokens::ID::RECT},
        {"CIRCLE",  Tokens::ID::CIRCLE}
        // ... and so on for all your keywords.
    };
} // end anonymous namespace

Tokens::ID Statements::get(const std::string& statement) {
    // Convert the input to uppercase to make the BASIC dialect case-insensitive.
    std::string upper_statement = to_upper(statement);

    // map.find() returns an iterator to the element, or map.end() if not found.
    auto it = keyword_map.find(upper_statement);

    if (it != keyword_map.end()) {
        // We found the keyword in the map, return its token.
        return it->second; // it->second is the value (the Token::ID)
    }

    // It's not a keyword.
    return Tokens::ID::NOCMD;
}