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
NeReLaBasic::NeReLaBasic() : memory(65536, 0) { // Allocate 64KB of memory
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
    pend = 0x0000; // A common start address for BASIC programs
    pcode = pend;

    TextIO::print("Prog start:   ");
    TextIO::print_uwhex(pcode);
    TextIO::nl();

    uint16_t free_mem = MEM_END - pcode;
    TextIO::print("Free memory:  ");
    TextIO::print_uw(free_mem);
    TextIO::nl();

    trace = 0;
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
        linenr = 0;
        TextIO::print("Ready\n? ");

        if (!std::getline(std::cin, inputLine) || inputLine.empty()) continue;

        // Tokenize the direct-mode line, passing '0' as the line number
        if (tokenize(inputLine, pend, 0) != 0) {
            Error::print();
            continue;
        }
        runl(); // Execute the line
        if (Error::get() != 0) {
            Error::print();
        }
    }
}

Tokens::ID NeReLaBasic::parse() {
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
        return parse(); // Call parse again to get the NOCMD token
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
// In NeReLaBasic.cpp

uint8_t NeReLaBasic::tokenize(const std::string& line, uint16_t baseAddress, uint16_t lineNumber) {
    // Set the global pointers to the start of this tokenization task
    lineinput = line;
    prgptr = 0;      // Start parsing from the beginning of the line
    pcode = baseAddress; // Start writing bytecode to this address
    
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
        return s;
    };
    
    // --- Write the 2-byte line number as a prefix to the bytecode ---
    memory[pcode++] = lineNumber & 0xFF;
    memory[pcode++] = (lineNumber >> 8) & 0xFF;

    // --- Check if this is a function definition line ---
    std::string quick_check_line = line;
    trim(quick_check_line);
    if (quick_check_line.rfind("func", 0) == 0) {
        // This line IS a function definition. We will parse it fully here
        // and generate only the necessary runtime bytecode.

        FunctionInfo info;

        size_t open_paren = line.find('(');
        size_t close_paren = line.rfind(')');

        if (open_paren != std::string::npos && close_paren != std::string::npos) {
            // Extract name
            std::string name_part = line.substr(0, open_paren);
            size_t name_start = name_part.find("func") + 4;
            info.name = name_part.substr(name_start);
            trim(info.name);

            // Extract params
            std::string params_str = line.substr(open_paren + 1, close_paren - (open_paren + 1));
            std::stringstream pss(params_str);
            std::string param;
            while (std::getline(pss, param, ',')) {
                trim(param);
                // --- THIS IS THE FIX FOR "fa()" ---
                size_t func_paren = param.find('(');
                if (func_paren != std::string::npos) {
                    param = param.substr(0, func_paren);
                }
                if (!param.empty()) info.parameter_names.push_back(to_upper(param));
            }

            // Populate the table entry completely
            info.start_pcode = pcode + 3; // The code starts after the FUNC token and its jump address
            function_table[to_upper(info.name)] = info;

            // Write the FUNC token and the placeholder for the jump-over address
            memory[pcode++] = static_cast<uint8_t>(Tokens::ID::FUNC);
            func_stack.push_back(pcode);
            pcode += 2;
        }
    }
    else {
        // This is a normal line, process it token by token.
        Tokens::ID token;
        do {
            token = parse(); // Get the next token from the line

            if (Error::get() != 0) {
                return Error::get(); // Abort on parsing error
            }

            if (token != Tokens::ID::NOCMD) {
                if (token == Tokens::ID::REM) {
                    // REM comment, ignore the rest of the line.
                    break;
                }
                if (token == Tokens::ID::ENDFUNC) {
                    // An endfunc is encountered. Write the token.
                    memory[pcode++] = static_cast<uint8_t>(token);
                    // Now, back-patch the FUNC's jump address.
                    if (!func_stack.empty()) {
                        uint16_t func_jump_addr = func_stack.back();
                        func_stack.pop_back();
                        // Patch the address to point to the current pcode (right after this ENDFUNC)
                        uint16_t jump_target = pcode;
                        memory[func_jump_addr] = jump_target & 0xFF;
                        memory[func_jump_addr + 1] = (jump_target >> 8) & 0xFF;
                    }
                    continue;
                }
                if (token == Tokens::ID::LABEL) {
                    // It's a label. Store its name and current address.
                    // The label itself is not written to the bytecode.
                    // TextIO::print("DEBUG: Registered label '" + buffer + "' at address " + std::to_string(pcode) + "\n");
                    label_addresses[buffer] = pcode;
                    continue; // Move to the next token on the line
                }
                if (token == Tokens::ID::THEN) {
                    // THEN is a compile-time marker used for grammar.
                    // We don't need to write it to the bytecode.
                    continue;
                }
                if (token == Tokens::ID::GOTO) {
                    // Write the GOTO command token itself
                    memory[pcode++] = static_cast<uint8_t>(token);

                    // Immediately parse the next token on the line, which should be the label name
                    Tokens::ID label_name_token = parse();

                    // Check if we got a valid identifier for the label
                    if (label_name_token == Tokens::ID::VARIANT || label_name_token == Tokens::ID::INT) {
                        // The buffer now holds the label name, so write IT to the bytecode
                        for (char c : buffer) {
                            memory[pcode++] = c;
                        }
                        memory[pcode++] = 0; // Null terminator
                    }
                    else {
                        // This is an error, e.g., "GOTO 123" or "GOTO +"
                        Error::set(1, runtime_current_line); // Syntax Error
                    }
                    // We have now processed GOTO and its argument, so restart the main loop
                    continue;
                }
                if (token == Tokens::ID::IF) {
                    // Write the IF token, then leave a 2-byte placeholder for the jump address.
                    memory[pcode++] = static_cast<uint8_t>(token);
                    if_stack.push_back({ pcode, current_source_line }); // Save address of the placeholder
                    pcode += 2; // Skip 2 bytes
                    continue; // The expression after IF will be tokenized next
                }
                if (token == Tokens::ID::ELSE) {
                    // Pop the IF's placeholder address from the stack.
                    IfStackInfo if_info = if_stack.back();
                    if_stack.pop_back();

                    // Now, write the ELSE token and its own placeholder for jumping past the ELSE block.
                    memory[pcode++] = static_cast<uint8_t>(token);
                    if_stack.push_back({ pcode, current_source_line }); // Push address of ELSE's placeholder
                    pcode += 2;

                    // Go back and patch the original IF's jump to point to the instruction AFTER the ELSE's placeholder.
                    uint16_t jump_target = pcode;
                    memory[if_info.patch_address] = jump_target & 0xFF;
                    memory[if_info.patch_address + 1] = (jump_target >> 8) & 0xFF;
                    continue;
                }
                if (token == Tokens::ID::ENDIF) {
                    // Pop the corresponding IF or ELSE placeholder address from the stack.
                    IfStackInfo last_if_info = if_stack.back();
                    if_stack.pop_back();

                    // Patch it to point to the current location.
                    uint16_t jump_target = pcode;
                    memory[last_if_info.patch_address] = jump_target & 0xFF;
                    memory[last_if_info.patch_address + 1] = (jump_target >> 8) & 0xFF;

                    // Don't write the ENDIF token, it's a compile-time marker.
                    continue;
                }
                if (token == Tokens::ID::TO || token == Tokens::ID::STEP) {
                    // These are compile-time markers for the FOR statement.
                    // We parse them but don't write them to the bytecode.
                    continue;
                }
                if (token == Tokens::ID::CALLFUNC) {
                    // The buffer holds the function name (e.g., "lall").
                    // Write the CALLFUNC token, then write the function name string.
                    memory[pcode++] = static_cast<uint8_t>(token);
                    for (char c : buffer) {
                        memory[pcode++] = c;
                    }
                    memory[pcode++] = 0; // Null terminator

                    // The arguments inside (...) will be tokenized as a normal expression
                    // by subsequent loops, which is what the evaluator expects.
                    continue;
                }

                // Write the token's ID to memory
                memory[pcode++] = static_cast<uint8_t>(token);

                // If the token has associated data (from the 'buffer'), process it.

                if (token == Tokens::ID::STRING) {
                    // Write the string characters to memory, followed by a null terminator
                    for (char c : buffer) {
                        memory[pcode++] = c;
                    }
                    memory[pcode++] = 0; // Null terminator
                }
                else if (token == Tokens::ID::NUMBER) {
                    try {
                        // Convert the string from the buffer to a full double
                        double value = std::stod(buffer);

                        // Copy the 8 raw bytes of the double into our bytecode
                        memcpy(&memory[pcode], &value, sizeof(double));

                        // Advance the program counter by the size of a double
                        pcode += sizeof(double);
                    }
                    catch (const std::exception&) {
                        // Handle cases where the number is invalid, e.g. "1.2.3"
                        Error::set(1, runtime_current_line); // Syntax Error
                    }
                }
                else if (!buffer.empty()) {
                    // All other tokens that use the buffer (VARIANT, STRVAR, etc.)
                    // only write their data if the buffer is not empty.
                    if (token == Tokens::ID::VARIANT ||
                        token == Tokens::ID::INT ||
                        token == Tokens::ID::STRVAR ||
                        token == Tokens::ID::FUNCREF ||
                        token == Tokens::ID::ARRAY_ACCESS)
                    {
                        for (char c : buffer) {
                            memory[pcode++] = c;
                        }
                        memory[pcode++] = 0;
                    }
                }
                //else if (token == Tokens::ID::VARIANT || token == Tokens::ID::INT ||
                //    token == Tokens::ID::STRVAR || token == Tokens::ID::ARRAY_ACCESS /* || add other variable types */) {
                //    // It's a variable. Write its name to memory, followed by a null terminator.
                //    for (char c : buffer) {
                //        memory[pcode++] = c;
                //    }
                //    memory[pcode++] = 0; // Null terminator
                //}
                //else if (token == Tokens::ID::FUNCREF) {
                //    // It's a function reference. Write its name to memory.
                //    for (char c : buffer) {
                //        memory[pcode++] = c;
                //    }
                //    memory[pcode++] = 0; // Null terminator
                //}

            }

        } while (token != Tokens::ID::NOCMD);
    }
    // After all tokens for the line are processed, append a C_CR token.
    // This marks the end of the current line in the bytecode stream.
    memory[pcode] = static_cast<uint8_t>(Tokens::ID::C_CR);
    pcode++;
    return 0; // Success
}

void NeReLaBasic::runl() {
    pcode = pend; // Set the program counter to the start of the freshly tokenized line
    Error::clear();

    // This is the execution loop for a single, direct-mode line.
    if (memory[pcode] == static_cast<uint8_t>(Tokens::ID::NOCMD)) return;

    runtime_current_line = memory[pcode] | (memory[pcode + 1] << 8);
    pcode += 2;

    while (true) {
        Tokens::ID token = static_cast<Tokens::ID>(memory[pcode]);
        if (token == Tokens::ID::C_CR || token == Tokens::ID::NOCMD || Error::get() != 0) {
            break;
        }
        statement();
    }
}

uint8_t NeReLaBasic::tokenize_program() {
    uint16_t source_ptr = PROGRAM_START_ADDR;
    pcode = pend;
    current_source_line = 1;

    std::string current_line;
    while (true) {
        if (memory[source_ptr] == static_cast<uint8_t>(Tokens::ID::NOCMD) && memory[source_ptr + 1] == 0) {
            break;
        }
        uint8_t c = memory[source_ptr++];
        if (c == '\n' || c == '\r') {
            if (!current_line.empty()) {
                // Tokenize the line we just read
                if (tokenize(current_line, pcode, current_source_line) != 0) {
                    Error::print();
                    return 1;
                }
                pcode = this->pcode; // Update pcode after tokenizing
                current_line.clear();
            }
            if (c == '\r' && memory[source_ptr] == '\n') {
                source_ptr++;
            }
            current_source_line++;
        }
        else {
            current_line += static_cast<char>(c);
        }
    }
    if (!current_line.empty()) {
        tokenize(current_line, pcode, current_source_line);
        pcode = this->pcode;
    }

    memory[pcode] = static_cast<uint8_t>(Tokens::ID::NOCMD);
    return 0;
}
void NeReLaBasic::run_program() {
    TextIO::print("Compiling...\n");
    // Step 0: Clear old state and set up for execution
    for_stack.clear();
    if_stack.clear();
    func_stack.clear();

    // Step 1: Compile the source at $A000 into bytecode at `pend`
    if (tokenize_program() != 0) {
        Error::print();
        return;
    }

    if (!if_stack.empty()) {
        // There are unclosed IF blocks. Get the line number of the last one.
        uint16_t error_line = if_stack.back().source_line;
        Error::set(4, error_line); // New Error: Missing ENDIF
        Error::print();
        return; // Stop before running faulty code
    }

    // Step 2: Clear old state and set up for execution
    variables.clear(); // Erase all old variables
    Error::clear();
    pcode = pend; // Set program counter to the start of the new bytecode

    TextIO::print("Running...\n");
    // Step 3: The main execution loop for a program
    while (true) {
        if (memory[pcode] == static_cast<uint8_t>(Tokens::ID::NOCMD)) break;

        // 1. Extract the line number prefix from the bytecode
        runtime_current_line = memory[pcode] | (memory[pcode + 1] << 8);
        pcode += 2;

        // 2. Execute statements until the end of the line (C_CR)
        while (static_cast<Tokens::ID>(memory[pcode]) != Tokens::ID::C_CR && static_cast<Tokens::ID>(memory[pcode]) != Tokens::ID::NOCMD) {
            statement();
            if (Error::get() != 0) break;
        }

        if (Error::get() != 0) {
            Error::print();
            break;
        }
        pcode++; // Consume the C_CR to move to the next line
    }
}

void NeReLaBasic::statement() {
    Tokens::ID token = static_cast<Tokens::ID>(memory[pcode]); // Peek at the token

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

    case Tokens::ID::RUN:
        pcode++;
        Commands::do_run(*this);
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
    Tokens::ID token = static_cast<Tokens::ID>(memory[pcode]);

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
        memcpy(&value, &memory[pcode], sizeof(double));

        // Advance the program counter by the size of a double
        pcode += sizeof(double);

        return value;
    }
    if (token == Tokens::ID::VARIANT || token == Tokens::ID::INT || token == Tokens::ID::STRVAR) { // Treat all numeric vars as float now
        pcode++;
        std::string var_name;
        while (memory[pcode] != 0) { var_name += memory[pcode++]; }
        pcode++;
        return get_variable(*this, to_upper(var_name));
    }
    if (token == Tokens::ID::FUNCREF) {
        pcode++; // Consume FUNCREF token
        std::string func_name = "";
        while (memory[pcode] != 0) { func_name += memory[pcode++]; }
        pcode++; // consume null

        // Return a BasicValue containing our new FunctionRef type
        return FunctionRef{ to_upper(func_name) };
    }
    if (token == Tokens::ID::STRING) {
        pcode++; // Consume STRING token
        std::string s_val;
        // Read the null-terminated string from bytecode
        while (memory[pcode] != 0) { s_val += memory[pcode++]; }
        pcode++; // consume null
        return s_val;
    }
    if (token == Tokens::ID::ARRAY_ACCESS) {
        pcode++;
        std::string array_name = to_upper(read_string(*this)); // Use a local helper

        //if (static_cast<Tokens::ID>(memory[pcode++]) != Tokens::ID::C_LEFTBRACKET) {
        //    Error::set(1, runtime_current_line); return false;
        //}
        BasicValue index_val = evaluate_expression();
        int index = static_cast<int>(to_double(index_val));
        if (static_cast<Tokens::ID>(memory[pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
            Error::set(1, runtime_current_line); return false;
        }
        if (!arrays.count(array_name) || index < 0 || index >= arrays.at(array_name).size()) {
            Error::set(24, runtime_current_line);
            return false;
        }
        return arrays.at(array_name)[index];
    }

    if (token == Tokens::ID::C_LEFTPAREN) {
        pcode++;
        auto result = evaluate_expression();
        if (static_cast<Tokens::ID>(memory[pcode]) != Tokens::ID::C_RIGHTPAREN) {
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
        std::string identifier_being_called = to_upper(read_string(*this)); // Use or create a helper for this
        std::string real_func_to_call = to_upper(identifier_being_called);

        // Check for higher-order function calls (a variable holding a FunctionRef)
        if (!function_table.count(real_func_to_call)) {
            if (get_variable(*this, identifier_being_called).index() == 3) { // 3 is the index for FunctionRef in our variant
                real_func_to_call = std::get<FunctionRef>(get_variable(*this, identifier_being_called)).name;
            }
        }

        if (!function_table.count(real_func_to_call)) {
            Error::set(22, runtime_current_line); return false;
        }

        const auto& func_info = function_table.at(real_func_to_call);

        // Is this a native C++ function?
        if (func_info.native_impl != nullptr) {
            std::vector<BasicValue> args;
            pcode++; // Skip '('

            bool first_arg = true;
            // Keep parsing expressions as long as we haven't hit the closing parenthesis
            while (pcode < memory.size() && static_cast<Tokens::ID>(memory[pcode]) != Tokens::ID::C_RIGHTPAREN) {
                if (!first_arg) {
                    if (static_cast<Tokens::ID>(memory[pcode]) == Tokens::ID::C_COMMA) {
                        pcode++; // Consume comma between arguments
                    }
                    else {
                        Error::set(1, runtime_current_line); // Syntax Error: Expected comma
                        return false;
                    }
                }
                args.push_back(evaluate_expression());
                if (Error::get() != 0) return false; // Stop on expression error
                first_arg = false;
            }
            pcode++; // Skip ')'

            // Perform an arity check for functions that expect a fixed number of arguments
            if (func_info.arity != -1 && args.size() != func_info.arity) {
                Error::set(26, runtime_current_line); // New Error: Wrong number of arguments
                return false;
            }
            // Execute the C++ function and return its result
            return func_info.native_impl(args);
        }
        else { // It's a user-defined BASIC function
            StackFrame frame;
            pcode++; // Skip '('
            for (const auto& param_name : func_info.parameter_names) {
                frame.local_variables[param_name] = evaluate_expression();
                if (static_cast<Tokens::ID>(memory[pcode]) == Tokens::ID::C_COMMA) pcode++;
            }
            pcode++; // Skip ')'

            frame.return_pcode = pcode;
            call_stack.push_back(frame);
            pcode = func_info.start_pcode;

            // Nested Execution Loop
            while (true) {
                Tokens::ID func_token = static_cast<Tokens::ID>(memory[pcode]);
                if (Error:: get() != 0) break;
                if (func_token == Tokens::ID::ENDFUNC || func_token == Tokens::ID::RETURN) {
                    statement(); break;
                }
                statement();
            }
            return variables["RETVAL"];
        }
    }    Error::set(1, runtime_current_line);
    return false;
}

// Level 4: Handles * and /
BasicValue NeReLaBasic::parse_factor() {
    BasicValue left = parse_primary();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>(memory[pcode]);
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
        Tokens::ID op = static_cast<Tokens::ID>(memory[pcode]);
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

    Tokens::ID op = static_cast<Tokens::ID>(memory[pcode]);

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
        Tokens::ID op = static_cast<Tokens::ID>(memory[pcode]);
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