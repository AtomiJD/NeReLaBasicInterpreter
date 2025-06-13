// NeReLaBasic.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "Types.hpp"
#include "Tokens.hpp"
#include <functional> 

// A type alias for our native C++ function pointers.
// All native functions will take a vector of arguments and return a single BasicValue.
using NativeFunction = std::function<BasicValue(const std::vector<BasicValue>&)>;

class NeReLaBasic {
public:
    // --- Member Variables (Global State) ---
    std::string buffer;
    std::string lineinput;
    std::string filename;

    uint16_t prgptr = 0;
    uint16_t pcode = 0;
    uint16_t linenr = 0;

    uint8_t graphmode = 0;
    uint8_t fgcolor = 5;
    uint8_t bgcolor = 0;
    uint8_t trace = 0;
    uint16_t runtime_current_line = 0;
    uint16_t current_source_line = 0;

    struct ForLoopInfo {
        std::string variable_name; // Name of the loop counter (e.g., "i")
        double end_value = 0;
        double step_value = 0;
        uint16_t loop_start_pcode = 0; // Address to jump back to on NEXT
    };

    struct FunctionInfo {
        std::string name;
        int arity = 0; // The number of arguments the function takes.
        bool is_procedure = false;

        // For user-defined functions:
        uint16_t start_pcode = 0;
        std::vector<std::string> parameter_names;

        // For native C++ functions:
        NativeFunction native_impl = nullptr; // A pointer to a C++ function
    };

    struct StackFrame {
        std::unordered_map<std::string, BasicValue> local_variables;
        uint16_t return_pcode = 0; // Where to jump back to after the function ends
    };

    struct IfStackInfo {
        uint16_t patch_address; // The address in the bytecode we need to patch
        uint16_t source_line;   // The source code line number of the IF statement
    };


    std::string source_code;      // Stores the BASIC program source text
    std::vector<uint8_t> program_p_code;    // Stores the compiled bytecode for RUN/DUMP
    std::vector<uint8_t> direct_p_code;     // Temporary buffer for direct-mode commands
    const std::vector<uint8_t>* active_p_code = nullptr;    //Active P-Code Pointer

    std::vector<IfStackInfo> if_stack;
    std::vector<ForLoopInfo> for_stack;

    std::unordered_map<std::string, FunctionInfo> function_table;
    std::vector<StackFrame> call_stack;
    std::vector<uint16_t> func_stack;

    // -- - Symbol Tables for Variables-- -
    std::unordered_map<std::string, BasicValue> variables;
    std::unordered_map<std::string, std::vector<BasicValue>> arrays;


    std::unordered_map<std::string, uint16_t> label_addresses;

    // --- Member Functions ---
    NeReLaBasic(); // Constructor
    void start();  // The main REPL
    void run_program();
    void execute(const std::vector<uint8_t>& code_to_run);


    // --- New Declarations for Expression Parsing ---
    BasicValue evaluate_expression();
    BasicValue parse_comparison();
    BasicValue parse_term();
    BasicValue parse_primary();
    BasicValue parse_factor();
    uint8_t tokenize_program(std::vector<uint8_t>& out_p_code);
    void statement();

private:
    void init_basic();
    void init_system();
    void init_screen();

    // --- Lexer---
    Tokens::ID parse(NeReLaBasic& vm, bool is_start_of_statement);
    uint8_t tokenize(const std::string& line, uint16_t lineNumber, std::vector<uint8_t>& out_p_code);

    // --- Execution Engine ---
    BasicValue execute_function_for_value(const FunctionInfo& func_info, const std::vector<BasicValue>& args);
};