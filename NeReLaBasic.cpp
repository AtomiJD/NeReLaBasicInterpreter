// NeReLaBasic.cpp
#include "Commands.hpp"
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
}

void NeReLaBasic::init_screen() {
    TextIO::setColor(fgcolor, bgcolor);
    TextIO::clearScreen();
    TextIO::print("NeReLa Basic v 0.6\n");
    TextIO::print("(c) 2024\n\n");
}

void NeReLaBasic::init_system() {
    // Let's set the program start memory location, as in the original.
    pend = 0x2000; // A common start address for BASIC programs
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

        if (!std::getline(std::cin, inputLine)) {
            break;
        }

        if (inputLine.empty()) {
            continue;
        }

        // 1. Tokenize the line into bytecode
        if (tokenize(inputLine, pend) != 0) {
            Error::print();
            continue; // Get next line of input
        }

        // 2. Execute the bytecode we just created
        runl();

        // 3. Print any runtime errors
        if (Error::get() != 0) {
            Error::print();
        }
    }
}

Tokens::ID NeReLaBasic::parse() {
    // This is a direct C++ translation of your 'parse' subroutine's logic.
    // We use the member 'lineinput' as the source and 'prgptr' as the current index.

    buffer.clear(); // Clear the shared buffer for the new token text.

    while (true) {
        // If we've reached the end of the input string
        if (prgptr >= lineinput.length()) {
            return Tokens::ID::NOCMD; // Or T_EOF, equivalent to your check for 0
        }

        char currentChar = lineinput[prgptr];

        // Check for end of line (like your $0D or $0A check)
        if (currentChar == '\n' || currentChar == '\r') {
            // In our getline version, we won't see these, but it's good practice
            return Tokens::ID::C_CR;
        }

        // Skip whitespace (like your string.isspace check)
        if (StringUtils::isspace(currentChar)) {
            prgptr++;
            continue; // Restart the loop at the next character
        }

        // Handle comments (apostrophe)
        if (currentChar == '\'') {
            // Skip until the end of the line
            while (prgptr < lineinput.length()) {
                prgptr++;
            }
            continue; // Look for the next token on the next line (or end)
        }

        // --- Token Identification ---

        // Is it a quoted string? (like your `if @(prgptr) == '"'`)
        if (currentChar == '"') {
            prgptr++; // Move past the opening quote
            size_t string_start_pos = prgptr;

            // Find the closing quote
            while (prgptr < lineinput.length() && lineinput[prgptr] != '"') {
                prgptr++;
            }

            if (prgptr < lineinput.length() && lineinput[prgptr] == '"') {
                // Found the closing quote. Extract the content.
                buffer = lineinput.substr(string_start_pos, prgptr - string_start_pos);
                prgptr++; // Move past the closing quote
                return Tokens::ID::STRING;
            }
            else {
                // Unterminated string
                Error::set(1); // Syntax Error
                return Tokens::ID::NOCMD;
            }
        }

        // Is it a number? (like your string.isdigit or '$' or '%' checks)
        if (StringUtils::isdigit(currentChar) ||
            (currentChar == '$' && prgptr + 1 < lineinput.length() && isxdigit(lineinput[prgptr + 1])) ||
            (currentChar == '%' && prgptr + 1 < lineinput.length() && (lineinput[prgptr + 1] == '0' || lineinput[prgptr + 1] == '1')))
        {
            size_t num_start_pos = prgptr;
            prgptr++; // Consume the first character

            // Keep consuming until a delimiter is found
            const std::string DELIMITERS = " =,;#()]*/-+%&><'";
            while (prgptr < lineinput.length() && !StringUtils::contains(DELIMITERS, lineinput[prgptr])) {
                prgptr++;
            }
            buffer = lineinput.substr(num_start_pos, prgptr - num_start_pos);
            return Tokens::ID::NUMBER;
        }

        // Is it a variable or a keyword? (like your string.isletter check)
        if (StringUtils::isletter(currentChar)) {
            size_t ident_start_pos = prgptr;
            // Consume all valid identifier characters (letters, digits, _)
            while (prgptr < lineinput.length() && (StringUtils::isletter(lineinput[prgptr]) || StringUtils::isdigit(lineinput[prgptr]) || lineinput[prgptr] == '_')) {
                prgptr++;
            }

            buffer = lineinput.substr(ident_start_pos, prgptr - ident_start_pos);

            // Skip any whitespace between the identifier and its suffix
            while (prgptr < lineinput.length() && StringUtils::isspace(lineinput[prgptr])) {
                prgptr++;
            }

            // --- KEYWORD CHECK ---
            // First, see if the identifier is a reserved keyword.
            Tokens::ID keywordToken = Statements::get(buffer);
            if (keywordToken != Tokens::ID::NOCMD) {
                // It's a keyword! Return the token for PRINT, IF, RUN, etc.
                buffer.clear(); // Keywords don't need a value in the buffer.
                return keywordToken;
            }

            // --- If not a keyword, it must be a variable or function call ---
            char suffix = (prgptr < lineinput.length()) ? lineinput[prgptr] : '\0';

            // This logic translates your series of 'if @(prgptr) == '$'' etc.
            switch (suffix) {
            case '$': prgptr++; return Tokens::ID::STRVAR;
            case '[': return Tokens::ID::ARRAY;
            case '(': return Tokens::ID::CALLFUNC;
            case ':': prgptr++; return Tokens::ID::LABEL;
            case '%': prgptr++; return Tokens::ID::INT;
            case '#': prgptr++; return Tokens::ID::FLOAT;
            case '!': prgptr++; return Tokens::ID::BYTE;
                // Add other suffixes like @ and & here

            default:
                // It's an identifier with no suffix, so it's a default variable type.
                return Tokens::ID::VARIANT;
            }
        }

        // --- If it's none of the above, check for single-character tokens ---
        // This part would translate your `singlechar` subroutine.
        // We can build a map or use a switch for this.
        switch (currentChar) {
        case ',': prgptr++; return Tokens::ID::C_COMMA;
        case ';': prgptr++; return Tokens::ID::C_SEMICOLON;
        case '+': prgptr++; return Tokens::ID::C_PLUS;
        case '-': prgptr++; return Tokens::ID::C_MINUS;
        case '*': prgptr++; return Tokens::ID::C_ASTR;
        case '/': prgptr++; return Tokens::ID::C_SLASH;
        case '(': prgptr++; return Tokens::ID::C_LEFTPAREN;
        case ')': prgptr++; return Tokens::ID::C_RIGHTPAREN;
        case '=': prgptr++; return Tokens::ID::C_EQ;
        case '<': prgptr++; return Tokens::ID::C_LT;
        case '>': prgptr++; return Tokens::ID::C_GT;
            // Add all other single characters here...
        }

        // If we get here, we have an unrecognized character
        prgptr++;
        Error::set(1); // Syntax Error
        return Tokens::ID::NOCMD;
    }
}

// In NeReLaBasic.cpp

uint8_t NeReLaBasic::tokenize(const std::string& line, uint16_t baseAddress) {
    // Set the global pointers to the start of this tokenization task
    lineinput = line;
    prgptr = 0;      // Start parsing from the beginning of the line
    pcode = baseAddress; // Start writing bytecode to this address

    Tokens::ID token;
    do {
        token = parse(); // Get the next token from the line

        if (Error::get() != 0) {
            return Error::get(); // Abort on parsing error
        }

        if (token != Tokens::ID::NOCMD) {
            if (token == Tokens::ID::LABEL) {
                // It's a label. Store its name and current address.
                // The label itself is not written to the bytecode.
                TextIO::print("DEBUG: Registered label '" + buffer + "' at address " + std::to_string(pcode) + "\n");
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
                    Error::set(1); // Syntax Error
                }
                // We have now processed GOTO and its argument, so restart the main loop
                continue;
            }
            // --- Start of IF/ELSE/ENDIF logic ---
            if (token == Tokens::ID::IF) {
                // Write the IF token, then leave a 2-byte placeholder for the jump address.
                memory[pcode++] = static_cast<uint8_t>(token);
                if_stack.push_back(pcode); // Save address of the placeholder
                pcode += 2; // Skip 2 bytes
                continue; // The expression after IF will be tokenized next
            }
            if (token == Tokens::ID::ELSE) {
                // Pop the IF's placeholder address from the stack.
                uint16_t if_jump_addr = if_stack.back();
                if_stack.pop_back();

                // Now, write the ELSE token and its own placeholder for jumping past the ELSE block.
                memory[pcode++] = static_cast<uint8_t>(token);
                if_stack.push_back(pcode); // Push address of ELSE's placeholder
                pcode += 2;

                // Go back and patch the original IF's jump to point to the instruction AFTER the ELSE's placeholder.
                uint16_t jump_target = pcode;
                memory[if_jump_addr] = jump_target & 0xFF;
                memory[if_jump_addr + 1] = (jump_target >> 8) & 0xFF;
                continue;
            }
            if (token == Tokens::ID::ENDIF) {
                // Pop the corresponding IF or ELSE placeholder address from the stack.
                uint16_t last_jump_addr = if_stack.back();
                if_stack.pop_back();

                // Patch it to point to the current location.
                uint16_t jump_target = pcode;
                memory[last_jump_addr] = jump_target & 0xFF;
                memory[last_jump_addr + 1] = (jump_target >> 8) & 0xFF;

                // Don't write the ENDIF token, it's a compile-time marker.
                continue;
            }
            // --- End of IF/ELSE/ENDIF logic ---
            // Write the token's ID to memory
            memory[pcode++] = static_cast<uint8_t>(token);

            // If the token has associated data (from the 'buffer'), process it.
            if (!buffer.empty()) {
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
                        Error::set(1); // Syntax Error
                    }
                }
                else if (token == Tokens::ID::VARIANT || token == Tokens::ID::INT ||
                    token == Tokens::ID::STRVAR /* || add other variable types */) {
                    // It's a variable. Write its name to memory, followed by a null terminator.
                    for (char c : buffer) {
                        memory[pcode++] = c;
                    }
                    memory[pcode++] = 0; // Null terminator
                }
            }
            else if (token == Tokens::ID::C_CR) {
                // The original code stores the line number after a carriage return
                linenr++; // Increment line number
                memory[pcode++] = linenr & 0xFF;
                memory[pcode++] = (linenr >> 8) & 0xFF;
            }
        }

    } while (token != Tokens::ID::NOCMD);

    // Write a final NOCMD to mark the end of the line's bytecode.
    memory[pcode] = static_cast<uint8_t>(Tokens::ID::NOCMD);

    return 0; // Success
}

// Add these new methods in NeReLaBasic.cpp

void NeReLaBasic::runl() {
    pcode = pend; // Set the program counter to the start of the freshly tokenized line
    Error::clear();

    // This is the execution loop for a single, direct-mode line.
    while (true) {
        Tokens::ID token = static_cast<Tokens::ID>(memory[pcode]);

        // A single line in direct mode always ends with NOCMD.
        if (token == Tokens::ID::NOCMD || Error::get() != 0) {
            break;
        }

        // The statement() function will now handle its own pcode advancement
        statement();
    }
}

uint8_t NeReLaBasic::tokenize_program() {
    uint16_t source_ptr = PROGRAM_START_ADDR;
    pcode = pend; // Start writing bytecode at the end of the interpreter
    linenr = 0;
    label_addresses.clear();
    std::string current_line;
    while (true) {
        uint8_t c = memory[source_ptr++];
        if (c == static_cast<uint8_t>(Tokens::ID::NOCMD) && memory[source_ptr] == 0) {
            // End of program source
            break;
        }

        if (c == '\n' || c == '\r') {
            if (!current_line.empty()) {
                tokenize(current_line, pcode); // Use existing line tokenizer
                pcode = this->pcode; // Update pcode from the tokenize call
                current_line.clear();
            }
        }
        else {
            current_line += static_cast<char>(c);
        }
    }
    // Tokenize any final line that didn't end with a newline
    if (!current_line.empty()) {
        tokenize(current_line, pcode);
        pcode = this->pcode;
    }

    // Write final end-of-bytecode marker
    memory[pcode] = static_cast<uint8_t>(Tokens::ID::NOCMD);
    return 0;
}


void NeReLaBasic::run_program() {
    TextIO::print("Compiling...\n");
    // Step 1: Compile the source at $A000 into bytecode at `pend`
    if (tokenize_program() != 0) {
        Error::print();
        return;
    }

    // Step 2: Clear old state and set up for execution
    variables.clear(); // Erase all old variables
    Error::clear();
    pcode = pend; // Set program counter to the start of the new bytecode

    TextIO::print("Running...\n");
    // Step 3: The main execution loop for a program
    while (true) {
        if (static_cast<Tokens::ID>(memory[pcode]) == Tokens::ID::NOCMD) break;
        statement(); // Let statement and its handlers manage pcode
        if (Error::get() != 0) break;
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
    case Tokens::ID::PRINT:
        pcode++;
        Commands::do_print(*this); // Pass a reference to ourselves
        break;

    case Tokens::ID::VARIANT:
    case Tokens::ID::INT:
        // case Tokens::ID::STRVAR: // For later
        pcode++;
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
        Error::set(1); // Syntax Error
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
    if (token == Tokens::ID::VARIANT || token == Tokens::ID::INT) { // Treat all numeric vars as float now
        pcode++;
        std::string var_name;
        while (memory[pcode] != 0) { var_name += memory[pcode++]; }
        pcode++;
        if (variables.count(var_name)) {
            return variables[var_name];
        }
        return 0.0; // Return 0.0 for uninitialized variables
    }
    if (token == Tokens::ID::C_LEFTPAREN) {
        pcode++;
        auto result = evaluate_expression();
        if (static_cast<Tokens::ID>(memory[pcode]) != Tokens::ID::C_RIGHTPAREN) {
            Error::set(1); return false;
        }
        pcode++;
        return result;
    }
    if (token == Tokens::ID::NOT) {
        pcode++;
        return !to_bool(parse_primary());
    }

    Error::set(1);
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
                if (r == 0.0) { Error::set(2); return false; }
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
            if (op == Tokens::ID::C_PLUS) left = to_double(left) + to_double(right);
            else left = to_double(left) - to_double(right);
        }
        else break;
    }
    return left;
}

// Level 2: Handles <, >, =
BasicValue NeReLaBasic::parse_comparison() {
    BasicValue left = parse_term();
    while (true) {
        Tokens::ID op = static_cast<Tokens::ID>(memory[pcode]);
        if (op == Tokens::ID::C_EQ || op == Tokens::ID::C_LT || op == Tokens::ID::C_GT) {
            pcode++;
            BasicValue right = parse_term();
            if (op == Tokens::ID::C_EQ) left = to_double(left) == to_double(right);
            else if (op == Tokens::ID::C_LT) left = to_double(left) < to_double(right);
            else left = to_double(left) > to_double(right);
        }
        else break;
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