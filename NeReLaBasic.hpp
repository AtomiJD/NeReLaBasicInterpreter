// NeReLaBasic.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "Types.hpp"
#include "Tokens.hpp"


class NeReLaBasic {
public:
    // --- Member Variables (Global State) ---
    static const uint16_t MEM_END = 0x9EFF;
    static const uint16_t PROGRAM_START_ADDR = 0xA000;

    std::string buffer;
    std::string lineinput;
    std::string filename;

    uint16_t prgptr = 0;
    uint16_t pcode = 0;
    uint16_t pend = 0;
    uint16_t linenr = 0;

    uint8_t graphmode = 0;
    uint8_t fgcolor = 5;
    uint8_t bgcolor = 0;
    uint8_t trace = 1;
    uint16_t runtime_current_line = 0;

    // Represents the 64KB memory space
    std::vector<uint8_t> memory;
    std::vector<uint16_t> if_stack;

    // -- - Symbol Tables for Variables-- -
    std::unordered_map<std::string, BasicValue> variables;
    std::unordered_map<std::string, uint16_t> label_addresses;

    // std::unordered_map<std::string, std::string> string_variables; // For later
    // Add maps for other types (float, etc.) as needed

    // --- Member Functions ---
    NeReLaBasic(); // Constructor
    void start();  // The main REPL
    void run_program();

    // --- New Declarations for Expression Parsing ---
    BasicValue evaluate_expression();
    BasicValue parse_comparison();
    BasicValue parse_term();
    BasicValue parse_primary();
    BasicValue parse_factor();

private:
    void init_basic();
    void init_system();
    void init_screen();

    // --- Lexer---
    Tokens::ID parse();
    uint8_t tokenize(const std::string& line, uint16_t baseAddress);
    uint8_t tokenize_program();

    // --- Execution Engine ---
    void runl();
    void statement();

};