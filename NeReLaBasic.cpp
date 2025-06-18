// NeReLaBasic.cpp
#include <sstream>     
#include "Commands.hpp"
#include "BuiltinFunctions.hpp" 
#include "Statements.hpp"
#include "NeReLaBasic.hpp"
#include "TextIO.hpp"
#include "Error.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include <fstream>   // For std::ifstream
#include <string>
#include <stdexcept>
#include <cstring>
#include <conio.h>
#include <algorithm> // for std::transform, std::find_if
#include <cctype>    // for std::isspace, std::toupper

const std::string NERELA_VERSION = "0.7";

void register_builtin_functions(NeReLaBasic& vm, NeReLaBasic::FunctionTable& table_to_populate);

namespace StringUtils {
    // Left trim
    static inline void ltrim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
            }));
    }

    // Right trim
    static inline void rtrim(std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
            }).base(), s.end());
    }

    // Trim from both ends
    void trim(std::string& s) {
        ltrim(s);
        rtrim(s);
    }

    // to_upper
    std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::toupper(c); });
        return s;
    }
} // namespace StringUtils


// Helper function to convert a string from the BASIC source to a number.
// Supports decimal, hexadecimal ('$'), and binary ('%').
uint16_t stringToWord(const std::string& s) {
    if (s.empty()) return 0;
    try {
        if (s[0] == '$') {
            return static_cast<uint16_t>(std::stoul(s.substr(1), nullptr, 16));
        }
        if (s[0] == '%') {
            return static_cast<uint16_t>(std::stoul(s.substr(1), nullptr, 2));
        }
        return static_cast<uint16_t>(std::stoul(s, nullptr, 10));
    }
    catch (const std::exception&) {
        // Handle cases where the number is invalid, e.g., "12A4"
        return 0;
    }
}

void trim(std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");

    if (start == std::string::npos)
        s.clear();  
    else
        s = s.substr(start, end - start + 1);
}

// Constructor: Initializes the interpreter state
NeReLaBasic::NeReLaBasic() : program_p_code(65536, 0) { // Allocate 64KB of memory
    buffer.reserve(64);
    lineinput.reserve(160);
    filename.reserve(40);
    active_function_table = &main_function_table;
    register_builtin_functions(*this, *active_function_table);
    srand(static_cast<unsigned int>(time(nullptr)));
}

bool NeReLaBasic::loadSourceFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile) {
        TextIO::print("Error: File not found -> " + filename + "\n");
        return false;
    }
    TextIO::print("LOADING " + filename + "\n");
    std::stringstream buffer;
    buffer << infile.rdbuf();
    source_code = buffer.str();
    return true;
}

void NeReLaBasic::init_screen() {
    TextIO::setColor(fgcolor, bgcolor);
    TextIO::clearScreen();
    TextIO::print("NeReLa Basic v " + NERELA_VERSION + "\n");
    TextIO::print("(c) 2025\n\n");
}

void NeReLaBasic::init_system() {
    // Let's set the program start memory location, as in the original.
    pcode = 0;

    TextIO::print("Prog start:   ");
    TextIO::print_uwhex(pcode);
    TextIO::nl();

    trace = 0;

    TextIO::print("Trace is:  ");
    TextIO::print_uw(trace);
    TextIO::nl();
}

void NeReLaBasic::init_basic() {
    TextIO::nl();
    //TextIO::print("Ready\n");
}

// The main REPL
void NeReLaBasic::start() {
    init_screen();
    init_system();
    init_basic();

    std::string inputLine;

    while (true) {
        Error::clear();
        direct_p_code.clear();
        linenr = 0;
        TextIO::print("Ready\n? ");

        if (!std::getline(std::cin, inputLine) || inputLine.empty()) {
            std::cin.clear();
            continue;
        }

        // -- - Special handling for RESUME-- -
        std::string temp_line = inputLine;
        StringUtils::trim(temp_line);
        if (StringUtils::to_upper(temp_line) == "RESUME") {
            if (is_stopped) {
                TextIO::print("Resuming...\n");
                is_stopped = false;
                execute(program_p_code); // Continues from the saved pcode
                if (Error::get() != 0) Error::print();
            }
            else {
                TextIO::print("?Nothing to resume.\n");
            }
            continue; // Go back to the REPL prompt
        }

        // Tokenize the direct-mode line, passing '0' as the line number
        active_function_table = &main_function_table;
        if (tokenize(inputLine, 0, direct_p_code, *active_function_table) != 0) {
            Error::print();
            continue;
        }
        // Execute the direct-mode p_code
        execute(direct_p_code);

        if (Error::get() != 0) {
            Error::print();
        }
    }
}

Tokens::ID NeReLaBasic::parse(NeReLaBasic& vm, bool is_start_of_statement) {
    buffer.clear();

    // --- Step 1: Skip any leading whitespace ---
    while (prgptr < lineinput.length() && StringUtils::isspace(lineinput[prgptr])) {
        prgptr++;
    }

    // --- Step 2: Check for end of line ---
    if (prgptr >= lineinput.length()) {
        return Tokens::ID::NOCMD;
    }

    // --- Step 3: Check for each token type in order ---
    char currentChar = lineinput[prgptr];

    // Handle Comments
    if (currentChar == '\'') {
        prgptr = lineinput.length(); // Skip to end of line
        return parse(*this, is_start_of_statement); // Call parse again to get the NOCMD token
    }

    // Handle String Literals (including "")
    if (currentChar == '"') {
        prgptr++; // Consume opening "
        size_t string_start_pos = prgptr;
        while (prgptr < lineinput.length() && lineinput[prgptr] != '"') {
            prgptr++;
        }
        if (prgptr < lineinput.length() && lineinput[prgptr] == '"') {
            buffer = lineinput.substr(string_start_pos, prgptr - string_start_pos);
            prgptr++; // Consume closing "
            return Tokens::ID::STRING;
        }
        else {
            Error::set(1, runtime_current_line); // Unterminated string
            return Tokens::ID::NOCMD;
        }
    }

    // Handle Numbers
    if (StringUtils::isdigit(currentChar) || (currentChar == '_' && StringUtils::isdigit(lineinput[prgptr + 1]))) {
        size_t num_start_pos = prgptr;
        while (prgptr < lineinput.length() && (StringUtils::isdigit(lineinput[prgptr]) || lineinput[prgptr] == '.')) {
            prgptr++;
        }
        buffer = lineinput.substr(num_start_pos, prgptr - num_start_pos);
        return Tokens::ID::NUMBER;
    }

    // Handle Identifiers (Variables, Keywords, Functions)
    if (StringUtils::isletter(currentChar)) {
        size_t ident_start_pos = prgptr;
        // Loop to capture the entire qualified name (e.g., "MATH.ADD" or "X")
        while (prgptr < lineinput.length()) {
            // Capture the current part of the identifier (e.g., "MATH" or "ADD")
            size_t part_start = prgptr;
            while (prgptr < lineinput.length() && (StringUtils::isletter(lineinput[prgptr]) || StringUtils::isdigit(lineinput[prgptr]) || lineinput[prgptr] == '_')) {
                prgptr++;
            }
            // If the part is empty, it's an error (e.g. "MODULE..FUNC") but we let it pass for now
            if (prgptr == part_start) break;

            // If we see a dot, loop again for the next part
            if (prgptr < lineinput.length() && lineinput[prgptr] == '.') {
                prgptr++;
            }
            else {
                // No more dots, so the full identifier is complete
                break;
            }
        }
        buffer = lineinput.substr(ident_start_pos, prgptr - ident_start_pos);
        buffer = to_upper(buffer);

        if (prgptr < lineinput.length() && lineinput[prgptr] == '$') {
            buffer += '$';
            prgptr++;
        }

        Tokens::ID keywordToken = Statements::get(buffer);
        if (keywordToken != Tokens::ID::NOCMD) {
            return keywordToken;
        }
        if (is_start_of_statement) {
            if (vm.active_function_table->count(buffer) && vm.active_function_table->at(buffer).is_procedure) {
                return Tokens::ID::CALLSUB; // It's a command-style procedure call!
            }
        }

        size_t suffix_ptr = prgptr;
        while (suffix_ptr < lineinput.length() && StringUtils::isspace(lineinput[suffix_ptr])) {
            suffix_ptr++;
        }
        char action_suffix = (suffix_ptr < lineinput.length()) ? lineinput[suffix_ptr] : '\0';

        if (is_start_of_statement && action_suffix == ':') {
            prgptr = suffix_ptr + 1;
            return Tokens::ID::LABEL;
        }
        if (action_suffix == '(') {
            prgptr = suffix_ptr;
            return Tokens::ID::CALLFUNC;
        }
        if (action_suffix == '[') {
            prgptr = suffix_ptr;
            return Tokens::ID::ARRAY_ACCESS;
        }
        if (action_suffix == '@') {
            prgptr = suffix_ptr + 1;
            return Tokens::ID::FUNCREF;
        }

        // If none of the above special cases match, it's a variable.
        if (buffer.back() == '$') {
            return Tokens::ID::STRVAR;
        }
        else {
            return Tokens::ID::VARIANT;
        }
    }

    // Handle Multi-Character Operators
    switch (currentChar) {
    case '<':
        if (prgptr + 1 < lineinput.length()) {
            if (lineinput[prgptr + 1] == '>') { prgptr += 2; return Tokens::ID::C_NE; }
            if (lineinput[prgptr + 1] == '=') { prgptr += 2; return Tokens::ID::C_LE; }
        }
        prgptr++; return Tokens::ID::C_LT;
    case '>':
        if (prgptr + 1 < lineinput.length() && lineinput[prgptr + 1] == '=') {
            prgptr += 2; return Tokens::ID::C_GE;
        }
        prgptr++; return Tokens::ID::C_GT;
    }

    // Handle all other single-character tokens
    prgptr++; // Consume the character
    switch (currentChar) {
    case ',': return Tokens::ID::C_COMMA;
    case ';': return Tokens::ID::C_SEMICOLON;
    case '+': return Tokens::ID::C_PLUS;
    case '-': return Tokens::ID::C_MINUS;
    case '*': return Tokens::ID::C_ASTR;
    case '/': return Tokens::ID::C_SLASH;
    case '(': return Tokens::ID::C_LEFTPAREN;
    case ')': return Tokens::ID::C_RIGHTPAREN;
    case '=': return Tokens::ID::C_EQ;
    case '[': return Tokens::ID::C_LEFTBRACKET;
    case ']': return Tokens::ID::C_RIGHTBRACKET;
    case ':': return Tokens::ID::C_COLON;
    }

    // If we get here, the character is not recognized.
    Error::set(1, current_source_line);
    return Tokens::ID::NOCMD;
}

uint8_t NeReLaBasic::tokenize(const std::string& line, uint16_t lineNumber, std::vector<uint8_t>& out_p_code, FunctionTable& compilation_func_table) {
    lineinput = line;
    prgptr = 0;

    // Write the line number prefix for this line's bytecode.
    out_p_code.push_back(lineNumber & 0xFF);
    out_p_code.push_back((lineNumber >> 8) & 0xFF);

    bool is_start_of_statement = true;
    bool is_one_liner_if = false;

    // Single loop to process all tokens on the line.
    while (prgptr < lineinput.length()) {

        bool is_exported = false;
        Tokens::ID token = parse(*this, is_start_of_statement);

        if (token == Tokens::ID::EXPORT) {
            is_exported = true;
            // It was an export, so get the *next* token (e.g., FUNC)
            token = parse(*this, false);
        }

        if (Error::get() != 0) return Error::get();
        if (token == Tokens::ID::NOCMD) break; // End of line reached.

        // Any token other than a comment or label means we are no longer at the start of a statement
        if (token == Tokens::ID::C_COLON) {
            is_start_of_statement = true;
        }
        else if (token != Tokens::ID::LABEL && token != Tokens::ID::REM) {
            is_start_of_statement = false;
        }

        // Use a switch for special compile-time tokens.
        switch (token) {
            // --- Ignore IMPORT and MODULE keywords during this phase ---
        case Tokens::ID::IMPORT:
        case Tokens::ID::MODULE:
            prgptr = lineinput.length(); // Skip the rest of the line
            continue;

            // Keywords that are ignored at compile-time (they are just markers).
        case Tokens::ID::TO:
        case Tokens::ID::STEP:
            continue; // Do nothing, just consume the token.

            // A comment token means we ignore the rest of the line.
        case Tokens::ID::REM:
            prgptr = lineinput.length();
            continue;

            // A label stores the current bytecode address but isn't a token itself.
        case Tokens::ID::LABEL:
            label_addresses[buffer] = out_p_code.size();
            continue;

            // Handle FUNC: parse the name, write a placeholder, and store the patch address.
        case Tokens::ID::FUNC: {
            parse(*this, is_start_of_statement); // Parse the next token, which is the function name.
            FunctionInfo info;
            info.name = to_upper(buffer);
            info.is_exported = is_exported;
            info.module_name = this->current_module_name;

            // Find parentheses to get parameter string
            size_t open_paren = line.find('(', prgptr);
            size_t close_paren = line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos) {
                auto trim = [](std::string& s) {
                    s.erase(0, s.find_first_not_of(" \t\n\r"));
                    s.erase(s.find_last_not_of(" \t\n\r") + 1);
                    };
                std::string params_str = line.substr(open_paren + 1, close_paren - (open_paren + 1));
                std::stringstream pss(params_str);
                std::string param;
                while (std::getline(pss, param, ',')) {
                    trim(param);
                    if (param.length() > 2 && param.substr(param.length() - 2) == "[]") {
                        param.resize(param.length() - 2);
                    }
                    if (!param.empty()) info.parameter_names.push_back(to_upper(param));
                }
            }

            info.arity = info.parameter_names.size();
            info.start_pcode = out_p_code.size() + 3; // +3 for FUNC token and 2-byte address
            compilation_func_table[info.name] = info;

            out_p_code.push_back(static_cast<uint8_t>(token));
            func_stack.push_back(out_p_code.size()); // Store address of the placeholder
            out_p_code.push_back(0); // Placeholder byte 1
            out_p_code.push_back(0); // Placeholder byte 2
            prgptr = lineinput.length(); // The rest of the line is params, so we consume it.
            continue;
        }

        // Handle ENDFUNC: pop the stored address and patch the jump offset.
        case Tokens::ID::ENDFUNC: {
            out_p_code.push_back(static_cast<uint8_t>(token));
            if (!func_stack.empty()) {
                uint16_t func_jump_addr = func_stack.back();
                func_stack.pop_back();
                uint16_t jump_target = out_p_code.size();
                out_p_code[func_jump_addr] = jump_target & 0xFF;
                out_p_code[func_jump_addr + 1] = (jump_target >> 8) & 0xFF;
            }
            continue;
        }
        case Tokens::ID::SUB: {
            // The 'SUB' token has been consumed. The next token must be the name.
            parse(*this, is_start_of_statement);
            FunctionInfo info;
            info.name = to_upper(buffer);
            info.is_procedure = true;
            info.is_exported = is_exported; // Set the exported status!
            info.module_name = this->current_module_name;

            // --- Use stringstream for robust parameter parsing ---
            // Find the rest of the line after the procedure name.
 /*           std::string all_params_str = lineinput.substr(prgptr);
            std::stringstream pss(all_params_str);
            std::string param_buffer;

            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\n\r"));
                s.erase(s.find_last_not_of(" \t\n\r") + 1);
                };*/

            // Read from the string stream, using ',' as a delimiter.
            //while (std::getline(pss, param_buffer, ',')) {
            //    trim(param_buffer);
            //    if (param_buffer.length() > 2 && param_buffer.substr(param_buffer.length() - 2) == "[]") {
            //        param_buffer.resize(param_buffer.length() - 2);
            //    }
            //    if (!param_buffer.empty()) {
            //        info.parameter_names.push_back(to_upper(param_buffer));
            //    }
            //}

            size_t open_paren = line.find('(', prgptr);
            size_t close_paren = line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos) {
                auto trim = [](std::string& s) {
                    s.erase(0, s.find_first_not_of(" \t\n\r"));
                    s.erase(s.find_last_not_of(" \t\n\r") + 1);
                    };
                std::string params_str = line.substr(open_paren + 1, close_paren - (open_paren + 1));
                std::stringstream pss(params_str);
                std::string param;
                while (std::getline(pss, param, ',')) {
                    trim(param);
                    if (param.length() > 2 && param.substr(param.length() - 2) == "[]") {
                        param.resize(param.length() - 2);
                    }
                    if (!param.empty()) info.parameter_names.push_back(to_upper(param));
                }
            }
            else {
                Error::set(1, current_source_line);
            }

            info.arity = info.parameter_names.size();
            info.start_pcode = out_p_code.size() + 3; // +3 for SUB token and 2-byte address
            compilation_func_table[info.name] = info;

            // Write the SUB token and its placeholder jump address
            out_p_code.push_back(static_cast<uint8_t>(token));
            func_stack.push_back(out_p_code.size());
            out_p_code.push_back(0); // Placeholder byte 1
            out_p_code.push_back(0); // Placeholder byte 2

            // We have processed the entire line.
            prgptr = lineinput.length();
            continue;
        }
        case Tokens::ID::ENDSUB: {
            out_p_code.push_back(static_cast<uint8_t>(token));
            if (!func_stack.empty()) {
                uint16_t func_jump_addr = func_stack.back();
                func_stack.pop_back();
                uint16_t jump_target = out_p_code.size();
                out_p_code[func_jump_addr] = jump_target & 0xFF;
                out_p_code[func_jump_addr + 1] = (jump_target >> 8) & 0xFF;
            }
            continue;
        }
        case Tokens::ID::CALLSUB: {
            out_p_code.push_back(static_cast<uint8_t>(token));
            for (char c : buffer) out_p_code.push_back(c);
            out_p_code.push_back(0); // Write the procedure name string
            // Arguments that follow will be tokenized normally.
            continue;
        }
        case Tokens::ID::GOTO: {
                // Write the GOTO command token itself
            out_p_code.push_back(static_cast<uint8_t>(token));

            // Immediately parse the next token on the line, which should be the label name
            Tokens::ID label_name_token = parse(*this, is_start_of_statement);

            // Check if we got a valid identifier for the label
            if (label_name_token == Tokens::ID::VARIANT || label_name_token == Tokens::ID::INT) {
                // The buffer now holds the label name, so write IT to the bytecode
                for (char c : buffer) {
                    out_p_code.push_back(c);
                }
                out_p_code.push_back(0); // Null terminator
            }
            else {
                // This is an error, e.g., "GOTO 123" or "GOTO +"
                Error::set(1, runtime_current_line); // Syntax Error
            }
            // We have now processed GOTO and its argument, so restart the main loop
            continue;
        }
        case Tokens::ID::IF: {
            // Write the IF token, then leave a 2-byte placeholder for the jump address.
            out_p_code.push_back(static_cast<uint8_t>(token));
            if_stack.push_back({ (uint16_t)out_p_code.size(), current_source_line }); // Save address of the placeholder
            out_p_code.push_back(0); // Placeholder byte 1
            out_p_code.push_back(0); // Placeholder byte 2
            break; // The expression after IF will be tokenized next
        }
        case Tokens::ID::THEN: {
            // This is the key decision point. We look ahead without consuming tokens.
            size_t peek_ptr = prgptr;
            while (peek_ptr < lineinput.length() && StringUtils::isspace(lineinput[peek_ptr])) {
                peek_ptr++;
            }
            // If nothing but a comment or whitespace follows THEN, it's a block IF.
            // Otherwise, it's a single-line IF.
            if (peek_ptr < lineinput.length() && lineinput[peek_ptr] != '\'') {
                is_one_liner_if = true;
            }
            // We never write the THEN token to bytecode, so we just continue.
            continue;
        }
        case Tokens::ID::ELSE: {
            // A single-line IF cannot have an ELSE clause.
            if (is_one_liner_if) {
                Error::set(1, current_source_line); // Syntax Error
                continue; // Stop processing this token
            }
            // Pop the IF's placeholder address from the stack.
            IfStackInfo if_info = if_stack.back();
            if_stack.pop_back();

            // Now, write the ELSE token and its own placeholder for jumping past the ELSE block.
            out_p_code.push_back(static_cast<uint8_t>(token));
            if_stack.push_back({ (uint16_t)out_p_code.size(), current_source_line }); // Push address of ELSE's placeholder
            out_p_code.push_back(0); // Placeholder byte 1
            out_p_code.push_back(0); // Placeholder byte 2
            //prgptr = lineinput.length(); 

            // Go back and patch the original IF's jump to point to the instruction AFTER the ELSE's placeholder.
            uint16_t jump_target = out_p_code.size();
            out_p_code[if_info.patch_address] = jump_target & 0xFF;
            out_p_code[if_info.patch_address + 1] = (jump_target >> 8) & 0xFF;
            continue;
        }
        case Tokens::ID::ENDIF: {
            // A single-line IF does not use an explicit ENDIF.
            if (is_one_liner_if) {
                Error::set(1, current_source_line); // Syntax Error
                continue; // Stop processing this token
            }
            // Pop the corresponding IF or ELSE placeholder address from the stack.
            IfStackInfo last_if_info = if_stack.back();
            if_stack.pop_back();

            // Patch it to point to the current location.
            uint16_t jump_target = out_p_code.size();
            out_p_code[last_if_info.patch_address] = jump_target & 0xFF;
            out_p_code[last_if_info.patch_address + 1] = (jump_target >> 8) & 0xFF;

            // Don't write the ENDIF token, it's a compile-time marker.
            continue;
        }
        case Tokens::ID::CALLFUNC: {
            // The buffer holds the function name (e.g., "lall").
            // Write the CALLFUNC token, then write the function name string.
            out_p_code.push_back(static_cast<uint8_t>(token));
            for (char c : buffer) {
                out_p_code.push_back(c);
            }
            out_p_code.push_back(0); // Null terminator

            // The arguments inside (...) will be tokenized as a normal expression
            // by subsequent loops, which is what the evaluator expects.
            continue;
        }
        case Tokens::ID::NEXT: {
            // Write the NEXT token to the P-Code. This is all the runtime needs.
            out_p_code.push_back(static_cast<uint8_t>(token));

            // Now, parse the variable name that follows ("i") to advance the
            // parsing pointer, but we will discard the result.
            parse(*this, is_start_of_statement);

            // We have now processed the entire "next i" statement.
            // Continue to the next token on the line (if any).
            continue;

        case Tokens::ID::AS:
            // This is a keyword we need at runtime for DIM.
            // Write the token to the bytecode stream.
            out_p_code.push_back(static_cast<uint8_t>(token));
            continue; // Continue to the next token
        }
        default: {
            out_p_code.push_back(static_cast<uint8_t>(token));
            if (token == Tokens::ID::STRING || token == Tokens::ID::VARIANT || token == Tokens::ID::INT ||
                token == Tokens::ID::STRVAR || token == Tokens::ID::FUNCREF || token == Tokens::ID::ARRAY_ACCESS ||
                token == Tokens::ID::CALLFUNC)
            {
                for (char c : buffer) out_p_code.push_back(c);
                out_p_code.push_back(0);
            }
            else if (token == Tokens::ID::NUMBER) {
                double value = std::stod(buffer);
                uint8_t double_bytes[sizeof(double)];
                memcpy(double_bytes, &value, sizeof(double));
                out_p_code.insert(out_p_code.end(), double_bytes, double_bytes + sizeof(double));
            }
        }
        }
    }
    if (is_one_liner_if) {
        if (!if_stack.empty()) {
            // This performs the ENDIF logic implicitly by patching the jump address.
            IfStackInfo last_if_info = if_stack.back();
            if_stack.pop_back();

            uint16_t jump_target = out_p_code.size();
            out_p_code[last_if_info.patch_address] = jump_target & 0xFF;
            out_p_code[last_if_info.patch_address + 1] = (jump_target >> 8) & 0xFF;
        }
        else {
            // This indicates a compiler logic error, but we can safeguard against it.
            Error::set(4, current_source_line); // Unclosed IF
        }
    }
    // Every line of bytecode ends with a carriage return token.
    out_p_code.push_back(static_cast<uint8_t>(Tokens::ID::C_CR));
    return 0; // Success
}

// --- HELPER FUNCTION TO COMPILE A MODULE FROM SOURCE ---
bool NeReLaBasic::compile_module(const std::string& module_name, const std::string& module_source_code) {
    if (this->compiled_modules.count(module_name)) {
        return true; // Already compiled
    }

    TextIO::print("Compiling dependent module: " + module_name + "\n");

    // --- NEW: No temporary compiler instance. ---
    // We compile the module using our own instance's state, but direct the output
    // into the module's own data structures within the `compiled_modules` map.

    // 1. Create the entry for the new module to hold its data.
    this->compiled_modules[module_name] = BasicModule{ module_name };

    // 2. Tokenize the module's source, telling the function where to put the results.
    // We pass the module's own p_code vector and function_table by reference.
    if (this->tokenize_program(this->compiled_modules[module_name].p_code, module_source_code) != 0) {
        Error::set(1, 0); // General compilation error
        return false;
    }
    else {
        TextIO::print("OK. Modul compiled to " + std::to_string(this->compiled_modules[module_name].p_code.size()) + " bytes.\n");
    }

    return true;
}

uint8_t NeReLaBasic::tokenize_program(std::vector<uint8_t>& out_p_code, const std::string& source) {
    // 1. Reset compiler state
    out_p_code.clear();
    if_stack.clear();
    func_stack.clear();
    label_addresses.clear();
    // We no longer clear the main function table here.

    // 2. Pre-scan to find imports and determine if we are a module
    is_compiling_module = false;
    current_module_name = "";
    std::vector<std::string> modules_to_import;
    std::stringstream pre_scan_stream(source);
    std::string line;
    while (std::getline(pre_scan_stream, line)) {
        StringUtils::trim(line);
        std::string line_upper = StringUtils::to_upper(line);
        if (line_upper.rfind("EXPORT MODULE", 0) == 0) {
            is_compiling_module = true;
            std::string temp = line_upper.substr(13);
            StringUtils::trim(temp);
            current_module_name = temp;
        }
        else if (line_upper.rfind("IMPORT", 0) == 0) {
            std::string temp = line_upper.substr(6);
            StringUtils::trim(temp);
            modules_to_import.push_back(temp);
        }
    }

    // 3. Get a pointer to the correct FunctionTable to populate.
    FunctionTable* target_func_table = nullptr;
    if (is_compiling_module) {
        // If we are a module, our target is our own table inside the modules map.
        target_func_table = &this->compiled_modules[current_module_name].function_table;
    }
    else {
        // If we are the main program, our target is the main function table.
        target_func_table = &this->main_function_table;
    }

    // 4. Clear and prepare the target table for compilation.
    target_func_table->clear();
    register_builtin_functions(*this, *target_func_table);

    FunctionTable* previous_active_table = this->active_function_table;
    this->active_function_table = target_func_table;

    // 5. Compile dependencies (must be done AFTER setting our active table)
    if (!is_compiling_module) {
        for (const auto& mod_name : modules_to_import) {
            if (compiled_modules.count(mod_name)) continue;
            std::string filename = mod_name + ".jdb";
            std::ifstream mod_file(filename);
            if (!mod_file) { Error::set(6, 0); TextIO::print("? Error: Module file not found: " + filename + "\n"); return 1; }
            std::stringstream buffer;
            buffer << mod_file.rdbuf();
            if (!compile_module(mod_name, buffer.str())) {
                TextIO::print("? Error: Failed to compile module: " + mod_name + "\n");
                return 1;
            }
        }
        is_compiling_module = false;
        current_module_name = "";
    }

    // 6. LINK FIRST, if this is the main program
    if (!is_compiling_module) {
        for (const auto& mod_name : modules_to_import) {
            if (!compiled_modules.count(mod_name)) continue;
            const BasicModule& mod = compiled_modules.at(mod_name);
            for (const auto& pair : mod.function_table) {
                if (pair.second.is_exported) {
                    std::string mangled_name = mod_name + "." + pair.first;
                    this->main_function_table[mangled_name] = pair.second;
                }
            }
        }
    }
    // 7. Main compilation loop
    std::stringstream source_stream(source);
    current_source_line = 1;
    while (std::getline(source_stream, line)) {
        //TextIO::print("C(" + std::to_string(current_source_line) + "): " + line + "\n");
        if (tokenize(line, current_source_line++, out_p_code, *target_func_table) != 0) {
            this->active_function_table = nullptr; // Reset on error
            return 1;
        }
    }

    // 8. Finalize p_code and linking
    out_p_code.push_back(0); out_p_code.push_back(0);
    out_p_code.push_back(static_cast<uint8_t>(Tokens::ID::NOCMD));

    if (!is_compiling_module) {
        // If we just compiled the main program, link the imported functions.
        for (const auto& mod_name : modules_to_import) {
            if (!compiled_modules.count(mod_name)) continue;

            const BasicModule& mod = compiled_modules.at(mod_name);
            for (const auto& pair : mod.function_table) {
                if (pair.second.is_exported) {
                    std::string mangled_name = mod_name + "." + pair.first;
                    this->main_function_table[mangled_name] = pair.second;
                }
            }
        }
    }

    this->active_function_table = previous_active_table;
    return 0;
}

BasicValue NeReLaBasic::execute_function_for_value(const FunctionInfo& func_info, const std::vector<BasicValue>& args) {
    if (func_info.native_impl != nullptr) {
        return func_info.native_impl(*this, args);
    }

    size_t initial_stack_depth = call_stack.size();

    // 1. Set up the new stack frame
    NeReLaBasic::StackFrame frame;
    frame.return_p_code_ptr = this->active_p_code;
    frame.return_pcode = this->pcode;
    frame.previous_function_table_ptr = this->active_function_table;
    frame.for_stack_size_on_entry = this->for_stack.size();

    for (size_t i = 0; i < func_info.parameter_names.size(); ++i) {
        if (i < args.size()) frame.local_variables[func_info.parameter_names[i]] = args[i];
    }
    call_stack.push_back(frame);

    // 2. --- CONTEXT SWITCH ---
    if (!func_info.module_name.empty() && compiled_modules.count(func_info.module_name)) {
        this->active_p_code = &this->compiled_modules.at(func_info.module_name).p_code;
        this->active_function_table = &this->compiled_modules.at(func_info.module_name).function_table;
    }
    // 3. Set the program counter to the function's start address IN THE NEW CONTEXT
    this->pcode = func_info.start_pcode;

    // 4. Execute statements until the function returns
    this->pcode = func_info.start_pcode;
    while (call_stack.size() > initial_stack_depth) {
        if (Error::get() != 0) {
            // Unwind stack on error to prevent infinite loops
            while (call_stack.size() > initial_stack_depth) call_stack.pop_back();
            this->active_function_table = frame.previous_function_table_ptr; // Restore context
            return false;
        }
        if (pcode >= active_p_code->size() || static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::NOCMD) {
            Error::set(25, runtime_current_line); // Missing RETURN
            while (call_stack.size() > initial_stack_depth) call_stack.pop_back();
            break;
        }
        statement();
    }
    return variables["RETVAL"];
}

void NeReLaBasic::execute(const std::vector<uint8_t>& code_to_run) {
    // If there's no code to run, do nothing.
    if (code_to_run.empty()) {
        return;
    }

    // Set the active p_code pointer for the duration of this execution.
    auto prev_active_p_code = active_p_code;
    active_p_code = &code_to_run;
    pcode = 0;
    Error::clear();

    // Main execution loop
    while (pcode < active_p_code->size()) {

#ifdef SDL3
        if (graphics_system.is_initialized) { // Check if graphics are active
            if (!graphics_system.handle_events()) {
                break; // Exit execution if the user closed the window
            }
        }
#endif
        if (_kbhit()) {
            char key = _getch(); // Get the pressed key

            // The ESC key has an ASCII value of 27
            if (key == 27) {
                TextIO::print("\n--- BREAK ---\n");
                break;                   // Exit the loop immediately
            }
            // Let's use the spacebar to pause
            else if (key == ' ') {
                TextIO::print("\n--- PAUSED (Press any key to resume) ---\n");
                _getch(); // Wait for another key press to un-pause
                TextIO::print("--- RESUMED ---\n");
            }
        }
        // Use the active_p_code pointer to access the bytecode

        runtime_current_line = (*active_p_code)[pcode] | ((*active_p_code)[pcode + 1] << 8);
        pcode += 2;

        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::NOCMD) {
            break;
        }
        bool line_is_done = false;
        while (!line_is_done && pcode < active_p_code->size()) {
            if (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_CR)
                statement();
            if (Error::get() != 0 || is_stopped) {
                line_is_done = true; // Stop processing this line on error
                continue;
            }

            // After the statement, check the token that follows.
            if (pcode < active_p_code->size()) {
                Tokens::ID next_token = static_cast<Tokens::ID>((*active_p_code)[pcode]);
                if (next_token == Tokens::ID::C_COLON) {
                    pcode++; // Consume the separator and loop again for the next statement.
                }
                else {
                    // Token (C_CR, NOCMD) means the line is finished.
                    if (next_token == Tokens::ID::C_CR || next_token == Tokens::ID::NOCMD) {
                        line_is_done = true;
                    }
                }
            }
        }

        if (Error::get() != 0 || is_stopped) {
            break;
        }

        // Check bounds before consuming C_CR
        //if (pcode < active_p_code->size() && static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::C_CR) {
        //    pcode++; // Consume the C_CR
        //}
        if (pcode < active_p_code->size() && (static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::C_CR || static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::NOCMD)) {
            pcode++; // Consume the C_CR
        }
    }

#ifdef SDL3
    // After the loop, ensure the graphics system is shut down
    graphics_system.shutdown();
#endif

    // Clear the pointer so it's not pointing to stale data
    active_p_code = prev_active_p_code;
}

void NeReLaBasic::statement() {
    Tokens::ID token = static_cast<Tokens::ID>((*active_p_code)[pcode]); // Peek at the token

    // In trace mode, print the token being executed.
    if (trace == 1) {
        TextIO::print("(");
        TextIO::print_uw(runtime_current_line);
        TextIO::print("/");
        TextIO::print_uwhex(static_cast<uint8_t>(token));
        TextIO::print(")");
    }

    switch (token) {

    case Tokens::ID::DIM:
        pcode++;
        Commands::do_dim(*this);
        break;

    case Tokens::ID::INPUT:
        pcode++;
        Commands::do_input(*this);
        break;

    case Tokens::ID::PRINT:
        pcode++;
        Commands::do_print(*this); // Pass a reference to ourselves
        break;

    case Tokens::ID::VARIANT:
    case Tokens::ID::INT:
    case Tokens::ID::STRVAR: 
    case Tokens::ID::ARRAY_ACCESS:
        //pcode++;
        Commands::do_let(*this);
        break;

    case Tokens::ID::GOTO:
        pcode++;
        Commands::do_goto(*this);
        break;

    case Tokens::ID::LABEL:
    case Tokens::ID::ENDIF:
        pcode++;
        // Compile-time only, do nothing at runtime.
        break;
    case Tokens::ID::IF:
        pcode++;
        Commands::do_if(*this); 
        break;
    case Tokens::ID::ELSE:
        pcode++;
        Commands::do_else(*this);
        break;
    case Tokens::ID::FOR:
        pcode++;
        Commands::do_for(*this);
        break;
    case Tokens::ID::NEXT:
        pcode++;
        Commands::do_next(*this);
        break;
    case Tokens::ID::FUNC:
        pcode++;
        Commands::do_func(*this);
        break;
    case Tokens::ID::CALLFUNC:
        pcode++;
        Commands::do_callfunc(*this);
        break;
    case Tokens::ID::ENDFUNC:
        pcode++;
        Commands::do_endfunc(*this);
        break;
    case Tokens::ID::RETURN:
        pcode++;
        Commands::do_return(*this);
        break;
    case Tokens::ID::SUB:
        pcode++;
        Commands::do_sub(*this);
        break;
    case Tokens::ID::ENDSUB:
        pcode++;
        Commands::do_endsub(*this);
        break;
    case Tokens::ID::CALLSUB:
        pcode++;
        Commands::do_callsub(*this);
        break;

    case Tokens::ID::EDIT:
        pcode++;
        Commands::do_edit(*this);
        break;

    case Tokens::ID::LIST:
        pcode++;
        Commands::do_list(*this);
        break;

    case Tokens::ID::LOAD:
        pcode++;
        Commands::do_load(*this);
        break;

    case Tokens::ID::SAVE:
        pcode++;
        Commands::do_save(*this);
        break;

    case Tokens::ID::COMPILE: 
        pcode++;
        Commands::do_compile(*this);
        break;

    case Tokens::ID::RUN:
        pcode++;
        Commands::do_run(*this);
        break;

    case Tokens::ID::STOP:
        pcode++;
        Commands::do_stop(*this);
        break;

    case Tokens::ID::TRON:
        pcode++;
        Commands::do_tron(*this);
        break;

    case Tokens::ID::TROFF:
        pcode++;
        Commands::do_troff(*this);
        break;

    case Tokens::ID::DUMP:
        pcode++;
        Commands::do_dump(*this);
        break;

    case Tokens::ID::C_CR:
        // This token is followed by a 2-byte line number. Skip it during execution.
        pcode++;
        runtime_current_line = (*active_p_code)[pcode] | ((*active_p_code)[pcode + 1] << 8);
        pcode += 2;
        break;

        // --- We will add more cases here for LET, IF, GOTO, etc. ---

    default:
        // If we don't recognize the token, something is wrong.
        // This could be a token that shouldn't be executed directly, like a NUMBER.
        // For now, we'll just stop. In the future, this would be a syntax error.
        pcode++;
        Error::set(1, runtime_current_line); // Syntax Error
        break;
    }
}

// --- CLASS MEMBER FUNCTION FOR PARSING ARRAY LITERALS ---
BasicValue NeReLaBasic::parse_array_literal() {
    // We expect the current token to be '['
    if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_LEFTBRACKET) {
        Error::set(1, runtime_current_line); // Should not happen if called correctly
        return {};
    }

    std::vector<BasicValue> elements;
    // Loop until we find the closing bracket ']'
    if (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_RIGHTBRACKET) {
        while (true) {
            // Recursively call evaluate_expression, which can now handle nested literals
            elements.push_back(evaluate_expression());
            if (Error::get() != 0) return {};

            Tokens::ID separator = static_cast<Tokens::ID>((*active_p_code)[pcode]);
            if (separator == Tokens::ID::C_RIGHTBRACKET) break;
            if (separator != Tokens::ID::C_COMMA) { Error::set(1, runtime_current_line); return {}; }
            pcode++;
        }
    }
    pcode++; // Consume ']'

    // --- Now, construct the Array object from the parsed elements ---
    auto new_array_ptr = std::make_shared<Array>();
    if (elements.empty()) {
        new_array_ptr->shape = { 0 };
        return new_array_ptr;
    }

    if (std::holds_alternative<std::shared_ptr<Array>>(elements[0])) {
        const auto& first_sub_array_ptr = std::get<std::shared_ptr<Array>>(elements[0]);
        new_array_ptr->shape.push_back(elements.size());
        if (first_sub_array_ptr) {
            for (size_t dim : first_sub_array_ptr->shape) {
                new_array_ptr->shape.push_back(dim);
            }
        }

        for (const auto& el : elements) {
            if (!std::holds_alternative<std::shared_ptr<Array>>(el)) { Error::set(15, runtime_current_line); return{}; }
            const auto& sub_array_ptr = std::get<std::shared_ptr<Array>>(el);
            if (!sub_array_ptr || sub_array_ptr->shape != first_sub_array_ptr->shape) { Error::set(15, runtime_current_line); return{}; }
            new_array_ptr->data.insert(new_array_ptr->data.end(), sub_array_ptr->data.begin(), sub_array_ptr->data.end());
        }
    }
    else {
        new_array_ptr->shape = { elements.size() };
        new_array_ptr->data = elements;
    }
    return new_array_ptr;
}

// Level 5: Handles highest-precedence items
BasicValue NeReLaBasic::parse_primary() {
    Tokens::ID token = static_cast<Tokens::ID>((*active_p_code)[pcode]);

    if (token == Tokens::ID::TRUE) {
        pcode++; return true;
    }
    if (token == Tokens::ID::FALSE) {
        pcode++; return false;
    }
    if (token == Tokens::ID::NUMBER) {
        pcode++; // Consume the NUMBER token
        double value;

        // Read the raw 8 bytes of the double from our bytecode
        memcpy(&value, &(*active_p_code)[pcode], sizeof(double));

        // Advance the program counter by the size of a double
        pcode += sizeof(double);

        return value;
    }
    if (token == Tokens::ID::VARIANT || token == Tokens::ID::INT || token == Tokens::ID::STRVAR) { // Treat all numeric vars as float now
        pcode++;
        std::string var_name;
        while ((*active_p_code)[pcode] != 0) { var_name += (*active_p_code)[pcode++]; }
        pcode++;
        return get_variable(*this, to_upper(var_name));
    }
    if (token == Tokens::ID::FUNCREF) {
        pcode++; // Consume FUNCREF token
        std::string func_name = "";
        while ((*active_p_code)[pcode] != 0) { func_name += (*active_p_code)[pcode++]; }
        pcode++; // consume null

        // Return a BasicValue containing our new FunctionRef type
        return FunctionRef{ to_upper(func_name) };
    }
    if (token == Tokens::ID::STRING) {
        pcode++; // Consume STRING token
        std::string s_val;
        // Read the null-terminated string from bytecode
        while ((*active_p_code)[pcode] != 0) { s_val += (*active_p_code)[pcode++]; }
        pcode++; // consume null
        return s_val;
    }
    if (token == Tokens::ID::ARRAY_ACCESS) {
        pcode++; // Consume ARRAY_ACCESS token
        std::string var_name = to_upper(read_string(*this));

        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_LEFTBRACKET) {
            Error::set(1, runtime_current_line); return false;
        }

        // --- Multi-dimensional Index Parsing ---
        std::vector<size_t> indices;
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_RIGHTBRACKET) {
            while (true) {
                BasicValue index_val = evaluate_expression();
                if (Error::get() != 0) return false;
                indices.push_back(static_cast<size_t>(to_double(index_val)));

                Tokens::ID separator = static_cast<Tokens::ID>((*active_p_code)[pcode]);
                if (separator == Tokens::ID::C_RIGHTBRACKET) break;
                if (separator != Tokens::ID::C_COMMA) { Error::set(1, runtime_current_line); return false; }
                pcode++;
            }
        }

        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
            Error::set(1, runtime_current_line); return false;
        }

        BasicValue& array_var = get_variable(*this, var_name);
        if (!std::holds_alternative<std::shared_ptr<Array>>(array_var)) {
            Error::set(15, runtime_current_line); return false; // Type Mismatch
        }
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(array_var);
        if (!arr_ptr) { Error::set(15, runtime_current_line); return false; }

        try {
            size_t flat_index = arr_ptr->get_flat_index(indices);
            return arr_ptr->data[flat_index];
        }
        catch (const std::exception&) {
            Error::set(10, runtime_current_line); // Bad subscript
            return false;
        }
    }
    if (token == Tokens::ID::C_LEFTBRACKET) {
        return parse_array_literal();
    }

    if (token == Tokens::ID::C_LEFTPAREN) {
        pcode++;
        auto result = evaluate_expression();
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_RIGHTPAREN) {
            Error::set(1, runtime_current_line); return false;
        }
        pcode++;
        return result;
    }
    //if (token == Tokens::ID::NOT) {
    //    pcode++;
    //    return !to_bool(parse_primary());
    //}
    if (token == Tokens::ID::CALLFUNC) {
        pcode++; // Consume CALLFUNC token
        std::string identifier_being_called = to_upper(read_string(*this));
        std::string real_func_to_call = identifier_being_called;

        // Check for higher-order function calls
        if (!active_function_table->count(real_func_to_call)) {
            BasicValue& var = get_variable(*this, identifier_being_called);
            if (std::holds_alternative<FunctionRef>(var)) {
                real_func_to_call = std::get<FunctionRef>(var).name;
            }
        }

        if (!active_function_table->count(real_func_to_call)) {
            Error::set(22, runtime_current_line);
            return false;
        }

        const auto& func_info = active_function_table->at(real_func_to_call);
        std::vector<BasicValue> args;

        // Argument Parsing Logic (This is now correct)
        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_LEFTPAREN) {
            Error::set(1, runtime_current_line); return false;
        }
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_RIGHTPAREN) {
            while (true) {
                // The ONLY rule is to evaluate the expression. This is the fix.
                args.push_back(evaluate_expression());
                if (Error::get() != 0) return false;

                Tokens::ID separator = static_cast<Tokens::ID>((*active_p_code)[pcode]);
                if (separator == Tokens::ID::C_RIGHTPAREN) break;
                if (separator != Tokens::ID::C_COMMA) { Error::set(1, runtime_current_line); return false; }
                pcode++;
            }
        }
        pcode++; // Consume ')'

        if (func_info.arity != -1 && args.size() != func_info.arity) {
            Error::set(26, runtime_current_line); return false;
        }

        // --- DELEGATE TO THE NEW HELPER FUNCTION ---
        return execute_function_for_value(func_info, args);
    }
    Error::set(1, runtime_current_line);
    return false;
}

// Level 5: Handles unary operators like - and NOT
BasicValue NeReLaBasic::parse_unary() {
    Tokens::ID token = static_cast<Tokens::ID>((*active_p_code)[pcode]);

    if (token == Tokens::ID::C_MINUS) {
        pcode++; // Consume the '-'
        // Recursively call to handle expressions like -(5+2) or -x
        // Note: The result of a unary minus is always a number.
        return -to_double(parse_unary());
    }
    if (token == Tokens::ID::NOT) {
        pcode++; // Consume 'NOT'
        // The result of NOT is always a boolean.
        return !to_bool(parse_unary());
    }

    // If there is no unary operator, parse the primary expression.
    return parse_primary();
}

// Level 4: Handles *, /, and MOD with element-wise array operations
BasicValue NeReLaBasic::parse_factor() {
    BasicValue left = parse_unary();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);
        if (op == Tokens::ID::C_ASTR || op == Tokens::ID::C_SLASH || op == Tokens::ID::MOD) {
            pcode++;
            BasicValue right = parse_unary();

            left = std::visit([op, this](auto&& l, auto&& r) -> BasicValue {
                using LeftT = std::decay_t<decltype(l)>;
                using RightT = std::decay_t<decltype(r)>;

                // Case 1: Array-Array operation
                if constexpr (std::is_same_v<LeftT, std::shared_ptr<Array>> && std::is_same_v<RightT, std::shared_ptr<Array>>) {
                    if (!l || !r) { Error::set(15, runtime_current_line); return false; } // Null array error
                    if (l->shape != r->shape) { Error::set(15, runtime_current_line); return false; } // Shape mismatch

                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = l->shape;
                    result_ptr->data.reserve(l->data.size());

                    for (size_t i = 0; i < l->data.size(); ++i) {
                        double left_val = to_double(l->data[i]);
                        double right_val = to_double(r->data[i]);
                        if (op == Tokens::ID::C_ASTR) result_ptr->data.push_back(left_val * right_val);
                        else if (op == Tokens::ID::C_SLASH) {
                            if (right_val == 0.0) { Error::set(2, runtime_current_line); return false; }
                            result_ptr->data.push_back(left_val / right_val);
                        }
                    }
                    return result_ptr;
                }
                // Case 2: Array-Scalar operation
                else if constexpr (std::is_same_v<LeftT, std::shared_ptr<Array>>) {
                    if (!l) { Error::set(15, runtime_current_line); return false; }
                    double scalar = to_double(r);
                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = l->shape;
                    result_ptr->data.reserve(l->data.size());
                    for (const auto& elem : l->data) {
                        if (op == Tokens::ID::C_ASTR) result_ptr->data.push_back(to_double(elem) * scalar);
                        else if (op == Tokens::ID::C_SLASH) {
                            if (scalar == 0.0) { Error::set(2, runtime_current_line); return false; }
                            result_ptr->data.push_back(to_double(elem) / scalar);
                        }
                    }
                    return result_ptr;
                }
                // Case 3: Scalar-Array operation
                else if constexpr (std::is_same_v<RightT, std::shared_ptr<Array>>) {
                    if (!r) { Error::set(15, runtime_current_line); return false; }
                    double scalar = to_double(l);
                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = r->shape;
                    result_ptr->data.reserve(r->data.size());
                    for (const auto& elem : r->data) {
                        if (op == Tokens::ID::C_ASTR) result_ptr->data.push_back(scalar * to_double(elem));
                        else if (op == Tokens::ID::C_SLASH) {
                            if (to_double(elem) == 0.0) { Error::set(2, runtime_current_line); return false; }
                            result_ptr->data.push_back(scalar / to_double(elem));
                        }
                    }
                    return result_ptr;
                }
                // Case 4: Fallback to simple scalar operation
                else {
                    if (op == Tokens::ID::C_ASTR) return to_double(l) * to_double(r);
                    if (op == Tokens::ID::C_SLASH) {
                        double right_val = to_double(r);
                        if (right_val == 0.0) { Error::set(2, runtime_current_line); return false; }
                        return to_double(l) / right_val;
                    }
                    if (op == Tokens::ID::MOD) {
                        long long left_val = static_cast<long long>(to_double(l));
                        long long right_val = static_cast<long long>(to_double(r));
                        if (right_val == 0) { Error::set(2, runtime_current_line); return false; }
                        return static_cast<double>(left_val % right_val);
                    }
                    return false; // Should not happen
                }
                }, left, right);
        }
        else break;
    }
    return left;
}

// Level 3: Handles + and - with element-wise array and string operations
BasicValue NeReLaBasic::parse_term() {
    BasicValue left = parse_factor();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);
        if (op == Tokens::ID::C_PLUS || op == Tokens::ID::C_MINUS) {
            pcode++;
            BasicValue right = parse_factor();

            left = std::visit([op, this](auto&& l, auto&& r) -> BasicValue {
                using LeftT = std::decay_t<decltype(l)>;
                using RightT = std::decay_t<decltype(r)>;

                // Case 1: Array-Array operation
                if constexpr (std::is_same_v<LeftT, std::shared_ptr<Array>> && std::is_same_v<RightT, std::shared_ptr<Array>>) {
                    if (!l || !r) { Error::set(15, runtime_current_line); return false; }
                    if (l->shape != r->shape) { Error::set(15, runtime_current_line); return false; }

                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = l->shape;
                    result_ptr->data.reserve(l->data.size());
                    for (size_t i = 0; i < l->data.size(); ++i) {
                        if (op == Tokens::ID::C_PLUS) result_ptr->data.push_back(to_double(l->data[i]) + to_double(r->data[i]));
                        else result_ptr->data.push_back(to_double(l->data[i]) - to_double(r->data[i]));
                    }
                    return result_ptr;
                }
                // Case 2: Array-Scalar operation
                else if constexpr (std::is_same_v<LeftT, std::shared_ptr<Array>>) {
                    if (!l) { Error::set(15, runtime_current_line); return false; }
                    double scalar = to_double(r);
                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = l->shape;
                    result_ptr->data.reserve(l->data.size());
                    for (const auto& elem : l->data) {
                        if (op == Tokens::ID::C_PLUS) result_ptr->data.push_back(to_double(elem) + scalar);
                        else result_ptr->data.push_back(to_double(elem) - scalar);
                    }
                    return result_ptr;
                }
                // Case 3: Scalar-Array operation
                else if constexpr (std::is_same_v<RightT, std::shared_ptr<Array>>) {
                    if (!r) { Error::set(15, runtime_current_line); return false; }
                    double scalar = to_double(l);
                    auto result_ptr = std::make_shared<Array>();
                    result_ptr->shape = r->shape;
                    result_ptr->data.reserve(r->data.size());
                    for (const auto& elem : r->data) {
                        if (op == Tokens::ID::C_PLUS) result_ptr->data.push_back(scalar + to_double(elem));
                        else result_ptr->data.push_back(scalar - to_double(elem));
                    }
                    return result_ptr;
                }
                // --- THIS IS THE FIX ---
                // First, check the TYPES at compile time.
                else if constexpr (std::is_same_v<LeftT, std::string> || std::is_same_v<RightT, std::string>) {
                    // Then, check the OPERATOR VALUE at runtime.
                    if (op == Tokens::ID::C_PLUS) {
                        return to_string(l) + to_string(r);
                    }
                    else { // Cannot subtract strings
                        Error::set(15, runtime_current_line); // Type Mismatch
                        return false;
                    }
                }
                // Case 5: Fallback to simple scalar operation
                else {
                    if (op == Tokens::ID::C_PLUS) return to_double(l) + to_double(r);
                    else return to_double(l) - to_double(r);
                }
                }, left, right);
        }
        else {
            break;
        }
    }
    return left;
}

// Level 2: Handles <, >, =
BasicValue NeReLaBasic::parse_comparison() {
    BasicValue left = parse_term(); // parse_term handles + and -

    Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);

    // Check if the next token is ANY of our comparison operators
    if (op == Tokens::ID::C_EQ || op == Tokens::ID::C_LT || op == Tokens::ID::C_GT ||
        op == Tokens::ID::C_NE || op == Tokens::ID::C_LE || op == Tokens::ID::C_GE)
    {
        pcode++; // Consume the operator
        BasicValue right = parse_term();

        // Use std::visit to handle string or number comparisons
        // We CAPTURE the original variants `left` and `right` to check their types.
        left = std::visit([op, &left, &right](auto&& l, auto&& r) -> BasicValue {

            // Check the type of the ORIGINAL variant objects.
            if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
                // If either is a string, we compare them as strings.
                if (op == Tokens::ID::C_EQ) return to_string(l) == to_string(r);
                if (op == Tokens::ID::C_NE) return to_string(l) != to_string(r);
                if (op == Tokens::ID::C_LT) return to_string(l) < to_string(r);
                if (op == Tokens::ID::C_GT) return to_string(l) > to_string(r);
                if (op == Tokens::ID::C_LE) return to_string(l) <= to_string(r);
                if (op == Tokens::ID::C_GE) return to_string(l) >= to_string(r);
            }
            // Priority 2: If BOTH operands are DateTime, compare their internal time_points.
            else if (std::holds_alternative<DateTime>(left) && std::holds_alternative<DateTime>(right)) {
                const auto& dt_l = std::get<DateTime>(left);
                const auto& dt_r = std::get<DateTime>(right);
                if (op == Tokens::ID::C_EQ) return dt_l.time_point == dt_r.time_point;
                if (op == Tokens::ID::C_NE) return dt_l.time_point != dt_r.time_point;
                if (op == Tokens::ID::C_LT) return dt_l.time_point < dt_r.time_point;
                if (op == Tokens::ID::C_GT) return dt_l.time_point > dt_r.time_point;
                if (op == Tokens::ID::C_LE) return dt_l.time_point <= dt_r.time_point;
                if (op == Tokens::ID::C_GE) return dt_l.time_point >= dt_r.time_point;
            }
            else {
                // Otherwise, both are numeric (double or bool), so we compare them as numbers.
                if (op == Tokens::ID::C_EQ) return to_double(l) == to_double(r);
                if (op == Tokens::ID::C_NE) return to_double(l) != to_double(r);
                if (op == Tokens::ID::C_LT) return to_double(l) < to_double(r);
                if (op == Tokens::ID::C_GT) return to_double(l) > to_double(r);
                if (op == Tokens::ID::C_LE) return to_double(l) <= to_double(r);
                if (op == Tokens::ID::C_GE) return to_double(l) >= to_double(r);
            }
            return false; // Should not be reached
            }, left, right);
    }

    return left;
}

// Level 1: Handles AND, OR
BasicValue NeReLaBasic::evaluate_expression() {
    BasicValue left = parse_comparison();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);
        if (op == Tokens::ID::AND || op == Tokens::ID::OR) {
            pcode++;
            BasicValue right = parse_comparison();
            if (op == Tokens::ID::AND) left = to_bool(left) && to_bool(right);
            else left = to_bool(left) || to_bool(right);
        }
        else break;
    }
    return left;
}