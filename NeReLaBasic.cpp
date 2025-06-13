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
#include <string>
#include <stdexcept>
#include <cstring>
#include <conio.h>

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

// Constructor: Initializes the interpreter state
NeReLaBasic::NeReLaBasic() : program_p_code(65536, 0) { // Allocate 64KB of memory
    buffer.reserve(64);
    lineinput.reserve(160);
    filename.reserve(40);
    register_builtin_functions(*this);
}

void NeReLaBasic::init_screen() {
    TextIO::setColor(fgcolor, bgcolor);
    TextIO::clearScreen();
    TextIO::print("NeReLa Basic v 0.6\n");
    TextIO::print("(c) 2024\n\n");
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

        if (!std::getline(std::cin, inputLine) || inputLine.empty()) continue;

        // Tokenize the direct-mode line, passing '0' as the line number
        if (tokenize(inputLine, 0, direct_p_code) != 0) {
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

Tokens::ID NeReLaBasic::parse(NeReLaBasic& vm) {
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
        return parse(*this); // Call parse again to get the NOCMD token
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
        while (prgptr < lineinput.length() && (lineinput[prgptr] == '_' || StringUtils::isletter(lineinput[prgptr]) || StringUtils::isdigit(lineinput[prgptr]))) {
            prgptr++;
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

        if (vm.function_table.count(buffer) && vm.function_table.at(buffer).is_procedure) {
            return Tokens::ID::CALLSUB; // It's a command-style procedure call!
        }

        size_t suffix_ptr = prgptr;
        while (suffix_ptr < lineinput.length() && StringUtils::isspace(lineinput[suffix_ptr])) {
            suffix_ptr++;
        }
        char action_suffix = (suffix_ptr < lineinput.length()) ? lineinput[suffix_ptr] : '\0';

        switch (action_suffix) {
        case '(':
            // It's a function call. The buffer already holds the full name (e.g., "INKEY$").
            prgptr = suffix_ptr; // Move pointer past any spaces
            return Tokens::ID::CALLFUNC;
        case '[':
            prgptr = suffix_ptr;
            return Tokens::ID::ARRAY_ACCESS;
        case '@':
            prgptr = suffix_ptr + 1;
            return Tokens::ID::FUNCREF;
        case ':':
            prgptr = suffix_ptr + 1;
            return Tokens::ID::LABEL;
        default:
            // It's a variable. The buffer already has the full name.
            // We just need to return the correct variable token type.
            if (buffer.back() == '$') {
                return Tokens::ID::STRVAR;
            }
            else {
                return Tokens::ID::VARIANT;
            }
        }

        //char suffix = (prgptr < lineinput.length()) ? lineinput[prgptr] : '\0';
        //switch (suffix) {
        //case '$': prgptr++; buffer += '$'; return Tokens::ID::STRVAR;
        //case '(': return Tokens::ID::CALLFUNC;
        //case '[': return Tokens::ID::ARRAY_ACCESS;
        //case '@': prgptr++; return Tokens::ID::FUNCREF;
        //case ':': prgptr++; return Tokens::ID::LABEL;
        //default: return Tokens::ID::VARIANT;
        //}
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
    }

    // If we get here, the character is not recognized.
    Error::set(1, current_source_line);
    return Tokens::ID::NOCMD;
}

uint8_t NeReLaBasic::tokenize(const std::string& line, uint16_t lineNumber, std::vector<uint8_t>& out_p_code) {
    lineinput = line;
    prgptr = 0;

    // Write the line number prefix for this line's bytecode.
    out_p_code.push_back(lineNumber & 0xFF);
    out_p_code.push_back((lineNumber >> 8) & 0xFF);

    // Single loop to process all tokens on the line.
    while (prgptr < lineinput.length()) {
        Tokens::ID token = parse(*this);

        if (Error::get() != 0) return Error::get();
        if (token == Tokens::ID::NOCMD) break; // End of line reached.

        // Use a switch for special compile-time tokens.
        switch (token) {
            // Keywords that are ignored at compile-time (they are just markers).
        case Tokens::ID::THEN:
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
            parse(*this); // Parse the next token, which is the function name.
            FunctionInfo info;
            info.name = to_upper(buffer);

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
                    if (!param.empty()) info.parameter_names.push_back(to_upper(param));
                }
            }

            info.arity = info.parameter_names.size();
            info.start_pcode = out_p_code.size() + 3; // +3 for FUNC token and 2-byte address
            function_table[info.name] = info;

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
            parse(*this);
            FunctionInfo info;
            info.name = to_upper(buffer);
            info.is_procedure = true;

            // --- Use stringstream for robust parameter parsing ---
            // Find the rest of the line after the procedure name.
            std::string all_params_str = lineinput.substr(prgptr);
            std::stringstream pss(all_params_str);
            std::string param_buffer;

            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\n\r"));
                s.erase(s.find_last_not_of(" \t\n\r") + 1);
                };

            // Read from the string stream, using ',' as a delimiter.
            while (std::getline(pss, param_buffer, ',')) {
                trim(param_buffer);
                if (!param_buffer.empty()) {
                    info.parameter_names.push_back(to_upper(param_buffer));
                }
            }

            info.arity = info.parameter_names.size();
            info.start_pcode = out_p_code.size() + 3; // +3 for SUB token and 2-byte address
            function_table[info.name] = info;

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
            Tokens::ID label_name_token = parse(*this);

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
            //prgptr = lineinput.length(); // The rest of the line is params, so we consume it.
            //prgptr = out_p_code.size(); // The rest of the line is params, so we consume it.
            break; // The expression after IF will be tokenized next
        }
        case Tokens::ID::ELSE: {
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
        if (token == Tokens::ID::CALLFUNC) {
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
            parse(*this);

            // We have now processed the entire "next i" statement.
            // Continue to the next token on the line (if any).
            continue;
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

    // Every line of bytecode ends with a carriage return token.
    out_p_code.push_back(static_cast<uint8_t>(Tokens::ID::C_CR));
    return 0; // Success
}

void NeReLaBasic::runl() {
    pcode = 0; 
    Error::clear();

    // Check if there is anything to execute
    if (direct_p_code.empty() || static_cast<Tokens::ID>(direct_p_code[pcode]) == Tokens::ID::NOCMD) {
        return;
    }

    // Read the line number (it's '0' for direct mode, but we must skip it)
    runtime_current_line = direct_p_code[pcode] | (direct_p_code[pcode + 1] << 8);
    pcode += 2;

    // Execute statements until the end of the line (C_CR) or an error occurs
    while (pcode < direct_p_code.size()) {
        Tokens::ID token = static_cast<Tokens::ID>(direct_p_code[pcode]);
        if (token == Tokens::ID::C_CR || token == Tokens::ID::NOCMD || Error::get() != 0) {
            break;
        }
        statement();
    }
}

uint8_t NeReLaBasic::tokenize_program(std::vector<uint8_t>& out_p_code) {
    out_p_code.clear(); // Start with empty bytecode
    if_stack.clear();
    func_stack.clear();
    label_addresses.clear();
    current_source_line = 1;

    std::stringstream source_stream(source_code);
    std::string current_line;

    while (std::getline(source_stream, current_line)) {
        // Handle potential '\r' at the end of lines
        if (!current_line.empty() && current_line.back() == '\r') {
            current_line.pop_back();
        }

        if (tokenize(current_line, current_source_line, out_p_code) != 0) {
            Error::print();
            return 1; // Tokenization failed
        }
        current_source_line++;
    }

    // Add final end-of-program marker
    out_p_code.push_back(static_cast<uint8_t>(Tokens::ID::NOCMD));

    return 0; // Success
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
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::NOCMD) {
            break;
        }

        runtime_current_line = (*active_p_code)[pcode] | ((*active_p_code)[pcode + 1] << 8);
        pcode += 2;

        while (pcode < active_p_code->size() && static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_CR) {
            statement();
            if (Error::get() != 0) break;
        }

        if (Error::get() != 0) {
            // Error::print() is now called by the callers (start() and do_run())
            break;
        }

        // Check bounds before consuming C_CR
        if (pcode < active_p_code->size()) {
            pcode++; // Consume the C_CR
        }
    }

    // Clear the pointer so it's not pointing to stale data
    active_p_code = prev_active_p_code;
}

void NeReLaBasic::run_program() {

    if (!if_stack.empty()) {
        // There are unclosed IF blocks. Get the line number of the last one.
        uint16_t error_line = if_stack.back().source_line;
        Error::set(4, error_line); // New Error: Missing ENDIF
        return; // Stop before running faulty code
    }

    // Setup for execution
    variables.clear();
    arrays.clear();
    call_stack.clear();
    for_stack.clear();
    Error::clear();
    pcode = 0; // Set program counter to the start of the bytecode

    TextIO::print("Running...\n");
    // Main execution loop
    while (pcode < program_p_code.size()) {
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::NOCMD) break;

        runtime_current_line = (*active_p_code)[pcode] | ((*active_p_code)[pcode + 1] << 8);
        pcode += 2;

        while (static_cast<Tokens::ID>((*active_p_code)[pcode]) != Tokens::ID::C_CR) {
            statement();
            if (Error::get() != 0) break;
        }

        if (Error::get() != 0) {
            Error::print();
            break;
        }
        pcode++; // Consume the C_CR
    }
}

void NeReLaBasic::statement() {
    Tokens::ID token = static_cast<Tokens::ID>((*active_p_code)[pcode]); // Peek at the token

    // In trace mode, print the token being executed.
    if (trace == 1) {
        TextIO::print("(");
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

// In NeReLaBasic.cpp, add these FIVE new functions.

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
        std::string base_name = to_upper(read_string(*this));

        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_LEFTBRACKET) {
            Error::set(1, runtime_current_line); // Syntax Error: expected '['
            return false;
        }

        // Evaluate the index expression
        BasicValue index_val = evaluate_expression();
        if (Error::get() != 0) return false;
        int index = static_cast<int>(to_double(index_val));

        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
            Error::set(1, runtime_current_line); return false;
        }

        // --- NEW LOGIC FOR ARRAY INDIRECTION ---
        std::string actual_array_name = base_name;

        // Check if `base_name` (e.g., "ARR") is a local variable that holds a string.
        // This will be true when we call `print_array arr`.
        BasicValue& var_lookup = get_variable(*this, base_name);
        if (std::holds_alternative<std::string>(var_lookup)) {
            // It is! The variable holds the name of the *real* array (e.g., "SOURCE_DATA").
            actual_array_name = to_upper(std::get<std::string>(var_lookup));
        }

        // Now, use the actual_array_name to access the global arrays map.
        if (arrays.find(actual_array_name) == arrays.end() || index < 0 || index >= arrays.at(actual_array_name).size()) {
            Error::set(24, runtime_current_line); // Bad subscript
            return false;
        }
        return arrays.at(actual_array_name)[index];
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
    if (token == Tokens::ID::NOT) {
        pcode++;
        return !to_bool(parse_primary());
    }
    if (token == Tokens::ID::CALLFUNC) {
        pcode++; // Consume CALLFUNC token
        std::string identifier_being_called = to_upper(read_string(*this));
        std::string real_func_to_call = identifier_being_called;

        // Check for higher-order function calls
        if (!function_table.count(real_func_to_call)) {
            BasicValue& var = get_variable(*this, identifier_being_called);
            if (std::holds_alternative<FunctionRef>(var)) {
                real_func_to_call = std::get<FunctionRef>(var).name;
            }
        }

        if (!function_table.count(real_func_to_call)) {
            Error::set(22, runtime_current_line);
            return false;
        }

        const auto& func_info = function_table.at(real_func_to_call);
        std::vector<BasicValue> args;

        // --- NEW, ROBUST ARGUMENT PARSING LOGIC ---
        if (static_cast<Tokens::ID>((*active_p_code)[pcode++]) != Tokens::ID::C_LEFTPAREN) {
            Error::set(1, runtime_current_line); return false;
        }

        // Check for an empty argument list: ()
        if (static_cast<Tokens::ID>((*active_p_code)[pcode]) == Tokens::ID::C_RIGHTPAREN) {
            pcode++; // Consume ')'
        }
        else {
            // Loop to parse one or more comma-separated arguments
            while (true) {
                // Parse the argument expression itself
                Tokens::ID arg_token = static_cast<Tokens::ID>((*active_p_code)[pcode]);
                if (arg_token == Tokens::ID::VARIANT || arg_token == Tokens::ID::STRVAR) {
                    pcode++;
                    args.push_back(read_string(*this));
                }
                else {
                    args.push_back(evaluate_expression());
                    if (Error::get() != 0) return false;
                }

                // After parsing the argument, we expect either a comma or a closing parenthesis
                Tokens::ID separator = static_cast<Tokens::ID>((*active_p_code)[pcode]);
                if (separator == Tokens::ID::C_RIGHTPAREN) {
                    pcode++; // Consume ')'
                    break;   // End of arguments
                }

                if (separator != Tokens::ID::C_COMMA) {
                    Error::set(1, runtime_current_line); // Syntax error: expected ',' or ')'
                    return false;
                }
                pcode++; // Consume ',' and loop for the next argument
            }
        }

        // Arity check
        if (func_info.arity != -1 && args.size() != func_info.arity) {
            Error::set(26, runtime_current_line); return false;
        }

        // --- Execution Logic for EXPRESSION context ---
        if (func_info.native_impl != nullptr) {
            return func_info.native_impl(args); // Native functions return immediately.
        }
        else {
            // For user functions, we must execute them now to get the return value.
            NeReLaBasic::StackFrame frame;
            for (size_t i = 0; i < func_info.parameter_names.size(); ++i) {
                if (i < args.size()) frame.local_variables[func_info.parameter_names[i]] = args[i];
            }

            frame.return_pcode = pcode;
            call_stack.push_back(frame);
            pcode = func_info.start_pcode;

            // Get the stack depth before we start executing the function.
            size_t initial_stack_depth = call_stack.size();

            // Loop as long as the function's frame is still on the stack.
            while (true) {
                if (Error::get() != 0) {
                    // An error occurred during function execution.
                    // Pop stack frames until we are back to the caller's context.
                    while (!call_stack.empty() && call_stack.back().return_pcode != frame.return_pcode) {
                        call_stack.pop_back();
                    }
                    return false; // Return a default error value
                }

                // If the stack is empty, something went wrong (e.g., ENDFUNC without RETURN)
                if (call_stack.empty()) break;

                // Check if the top stack frame's return address is OUR return address.
                // If it is NOT, it means the function we called has returned.
                if (call_stack.back().return_pcode != frame.return_pcode) {
                    break;
                }

                statement();
            }
            // The RETURN statement has set variables["RETVAL"].
            return variables["RETVAL"];
        }
    }    Error::set(1, runtime_current_line);
    return false;
}

// Level 4: Handles * and /
BasicValue NeReLaBasic::parse_factor() {
    BasicValue left = parse_primary();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);
        if (op == Tokens::ID::C_ASTR || op == Tokens::ID::C_SLASH) {
            pcode++;
            BasicValue right = parse_primary();
            if (op == Tokens::ID::C_ASTR) left = to_double(left) * to_double(right);
            else {
                double r = to_double(right);
                if (r == 0.0) { Error::set(2, runtime_current_line); return false; }
                left = to_double(left) / r;
            }
        }
        else break;
    }
    return left;
}

// Level 3: Handles + and -
BasicValue NeReLaBasic::parse_term() {
    BasicValue left = parse_factor();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>((*active_p_code)[pcode]);
        if (op == Tokens::ID::C_PLUS || op == Tokens::ID::C_MINUS) {
            pcode++;
            BasicValue right = parse_factor();

            // Use std::visit to handle different type combinations
            left = std::visit([op](auto&& l, auto&& r) -> BasicValue {
                // If either operand is a string, do string concatenation.
                if constexpr (std::is_same_v<std::decay_t<decltype(l)>, std::string> ||
                    std::is_same_v<std::decay_t<decltype(r)>, std::string>) {
                    if (op == Tokens::ID::C_PLUS) {
                        return to_string(l) + to_string(r);
                    }
                    else {
                        // Subtraction on strings is a syntax error
                        return false; // Placeholder for error
                    }
                }
                // Otherwise, perform numeric arithmetic.
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