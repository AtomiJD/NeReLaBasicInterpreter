// NeReLaBasic.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <unordered_map>
#include "Types.hpp"
#include "Tokens.hpp"
#include <functional> 

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
    uint8_t fgcolor = 2;
    uint8_t bgcolor = 0;
    uint8_t trace = 0;
    bool is_stopped = false;

    uint16_t runtime_current_line = 0;
    uint16_t current_source_line = 0;

    struct ForLoopInfo {
        std::string variable_name; // Name of the loop counter (e.g., "i")
        double end_value = 0;
        double step_value = 0;
        uint16_t loop_start_pcode = 0; // Address to jump back to on NEXT
    };

    // A type alias for our native C++ function pointers.
    // All native functions will take a vector of arguments and return a single BasicValue.
    using NativeFunction = std::function<BasicValue(NeReLaBasic&, const std::vector<BasicValue>&)>;

    struct FunctionInfo {
        std::string name;
        int arity = 0; // The number of arguments the function takes.
        bool is_procedure = false;
        bool is_exported = false;
        std::string module_name;
        uint16_t start_pcode = 0;
        std::vector<std::string> parameter_names;
        NativeFunction native_impl = nullptr; // A pointer to a C++ function
    };

    using FunctionTable = std::unordered_map<std::string, NeReLaBasic::FunctionInfo>;

    struct StackFrame {
//        std::string return_module_name;
        //std::map<std::string, FunctionInfo> original_function_table;
        std::unordered_map<std::string, BasicValue> local_variables;
        uint16_t return_pcode = 0; // Where to jump back to after the function ends
        const std::vector<uint8_t>* return_p_code_ptr;
        FunctionTable* previous_function_table_ptr;
        size_t for_stack_size_on_entry;
    };

    struct IfStackInfo {
        uint16_t patch_address; // The address in the bytecode we need to patch
        uint16_t source_line;   // The source code line number of the IF statement
    };

    struct BasicModule {
        std::string name;
        std::vector<uint8_t> p_code;
        FunctionTable function_table;
    };


    std::string source_code;      // Stores the BASIC program source text
    std::vector<std::string> source_lines;
    std::vector<uint8_t> program_p_code;    // Stores the compiled bytecode for RUN/DUMP
    std::vector<uint8_t> direct_p_code;     // Temporary buffer for direct-mode commands
    const std::vector<uint8_t>* active_p_code = nullptr;    //Active P-Code Pointer

    std::vector<IfStackInfo> if_stack;
    std::vector<ForLoopInfo> for_stack;

    //std::unordered_map<std::string, FunctionInfo> function_table;
    std::vector<StackFrame> call_stack;
    std::vector<uint16_t> func_stack;

    // The main program has its own function table
    FunctionTable main_function_table;

    // Each compiled module's function table is stored here
    std::map<std::string, FunctionTable> module_function_tables;

    // This single pointer represents the currently active function table.
    // At runtime, we just change what this pointer points to.
    FunctionTable* active_function_table = nullptr;

    // -- - Symbol Tables for Variables-- -
    std::unordered_map<std::string, BasicValue> variables;
    std::unordered_map<std::string, DataType> variable_types;

    std::unordered_map<std::string, uint16_t> label_addresses;

    // --- C++ Modules ---
    std::map<std::string, BasicModule> compiled_modules;
    // True if the compiler is currently processing a module file
    bool is_compiling_module = false;
    // Holds the name of the module currently being compiled
    std::string current_module_name;

    // --- Member Functions ---
    NeReLaBasic(); // Constructor
    void start();  // The main REPL
    void execute(const std::vector<uint8_t>& code_to_run);
    bool loadSourceFromFile(const std::string& filename);


    // --- New Declarations for Expression Parsing ---
    BasicValue evaluate_expression();
    BasicValue parse_comparison();
    BasicValue parse_term();
    BasicValue parse_primary();
    BasicValue parse_unary();
    BasicValue parse_factor();
    BasicValue parse_array_literal();
    bool compile_module(const std::string& module_name, const std::string& module_source_code);
    uint8_t tokenize_program(std::vector<uint8_t>& out_p_code, const std::string& source);
    void statement();

private:
    void init_basic();
    void init_system();
    void init_screen();

    // --- Lexer---
    Tokens::ID parse(NeReLaBasic& vm, bool is_start_of_statement);
    uint8_t tokenize(const std::string& line, uint16_t lineNumber, std::vector<uint8_t>& out_p_code, FunctionTable& compilation_func_table);

    // --- Execution Engine ---
    BasicValue execute_function_for_value(const FunctionInfo& func_info, const std::vector<BasicValue>& args);
};