// Commands.cpp
#include <fstream> 
#include "Commands.hpp"
#include "NeReLaBasic.hpp" // We need the full class definition here
#include "TextIO.hpp"
#include "Tokens.hpp"
#include "Error.hpp"
#include "Types.hpp"
#include <iostream>

namespace {
    // Helper to read a 16-bit word from memory (little-endian)
    uint16_t read_word(NeReLaBasic& vm) {
        uint8_t lsb = vm.memory[vm.pcode++];
        uint8_t msb = vm.memory[vm.pcode++];
        return (msb << 8) | lsb;
    }

    // Helper to read a null-terminated string from memory
    std::string read_string(NeReLaBasic& vm) {
        std::string s;
        while (vm.memory[vm.pcode] != 0) {
            s += vm.memory[vm.pcode++];
        }
        vm.pcode++; // Skip the null terminator
        return s;
    }
}

void print_value(const BasicValue& val) {
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            TextIO::print(arg ? "TRUE" : "FALSE");
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Convert double to string for printing
            std::string s = std::to_string(arg);
            // Remove trailing zeros
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            // Remove trailing decimal point if it exists
            if (s.back() == '.') {
                s.pop_back();
            }
            TextIO::print(s);
        }
        }, val);
}

void Commands::do_print(NeReLaBasic& vm) {
    // Loop through all items in the PRINT list until the statement ends.
    while (true) {
        // Peek at the next token to decide what to do.
        Tokens::ID next_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
        if (next_token == Tokens::ID::NOCMD || next_token == Tokens::ID::C_CR) {
            TextIO::nl(); return;
        }

        if (next_token == Tokens::ID::STRING) {
            vm.pcode++;
            TextIO::print(read_string(vm));
        }
        else 
        if (next_token == Tokens::ID::NUMBER || next_token == Tokens::ID::VARIANT || next_token == Tokens::ID::INT) {
            BasicValue result = vm.evaluate_expression();
            if (Error::get() != 0) return;
            print_value(result); // Use our new helper to print any type
        }
        else {
            if (Error::get() != 0) return;
        }


        // --- Step 2: Look ahead for a separator ---
        Tokens::ID separator = static_cast<Tokens::ID>(vm.memory[vm.pcode]);

        if (separator == Tokens::ID::C_COMMA) {
            vm.pcode++; // Consume the comma
            // Check if the line ends right after the comma
            Tokens::ID after_separator = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
            if (after_separator == Tokens::ID::NOCMD || after_separator == Tokens::ID::C_CR) {
                return; // Ends with a comma, so no newline.
            }
            TextIO::print("\t"); // Otherwise, print a tab and continue the loop.
        }
        else if (separator == Tokens::ID::C_SEMICOLON) {
            vm.pcode++; // Consume the semicolon
            // Check if the line ends right after the semicolon
            Tokens::ID after_separator = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
            if (after_separator == Tokens::ID::NOCMD || after_separator == Tokens::ID::C_CR) {
                return; // Ends with a semicolon, so no newline.
            }
            // Otherwise, just continue the loop to the next item.
        }
        else {
            // No separator after the item, so the statement is over.
            TextIO::nl();
            return;
        }
    }
}
void Commands::do_let(NeReLaBasic& vm) {
    std::string var_name;
    while (vm.memory[vm.pcode] != 0) { var_name += vm.memory[vm.pcode++]; }
    vm.pcode++;
    if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) != Tokens::ID::C_EQ) {
        Error::set(1); return;
    }
    vm.pcode++;
    BasicValue value = vm.evaluate_expression(); // This now returns a BasicValue
    if (Error::get() != 0) return;
    vm.variables[var_name] = value; // Store the BasicValue in the new map
}

void Commands::do_goto(NeReLaBasic& vm) {
    // The label name was stored as a string in the bytecode after the GOTO token.
    std::string label_name = read_string(vm);

    // Look up the label in the address map
    if (vm.label_addresses.count(label_name)) {
        // Found it! Set the program counter to the stored address.
        uint16_t target_address = vm.label_addresses[label_name];
        vm.pcode = target_address;
    }
    else {
        // Error: Label not found
        TextIO::print("? UNDEFINED LABEL ERROR\n");
        Error::set(20); // Use a new error code
    }
}

void Commands::do_if(NeReLaBasic& vm) {
    // The IF token has already been consumed by `statement`. `pcode` points to the 2-byte placeholder.
    uint16_t jump_placeholder_addr = vm.pcode;
    vm.pcode += 2; // Skip the placeholder to get to the expression.

    BasicValue result = vm.evaluate_expression();
    if (Error::get() != 0) return;

    if (!to_bool(result)) {
        // Condition is false, so jump. Read the address from the placeholder.
        uint8_t lsb = vm.memory[jump_placeholder_addr];
        uint8_t msb = vm.memory[jump_placeholder_addr + 1];
        vm.pcode = (msb << 8) | lsb;
    }
    // If true, we do nothing and just continue execution from the current pcode.
}

// Correct do_else implementation
void Commands::do_else(NeReLaBasic& vm) {
    // ELSE is an unconditional jump. The placeholder is right after the token.
    uint16_t jump_placeholder_addr = vm.pcode;
    uint8_t lsb = vm.memory[jump_placeholder_addr];
    uint8_t msb = vm.memory[jump_placeholder_addr + 1];
    vm.pcode = (msb << 8) | lsb;
}


void Commands::do_list(NeReLaBasic& vm) {
    uint16_t ptr = vm.PROGRAM_START_ADDR;
    while (true) {
        uint8_t token = vm.memory[ptr];
        // The end-of-program marker is a NOCMD token followed by a null byte.
        if (token == static_cast<uint8_t>(Tokens::ID::NOCMD) && vm.memory[ptr + 1] == 0) {
            break;
        }
        // Just print the character as-is.
        std::cout << static_cast<char>(token);
        ptr++;
        // Safety break to prevent infinite loops on bad programs
        if (ptr >= vm.memory.size() - 1) break;
    }
}

void Commands::do_load(NeReLaBasic& vm) {
    // LOAD expects a filename in a string literal
    if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) != Tokens::ID::STRING) {
        Error::set(1); // Syntax Error
        TextIO::print("?SYNTAX ERROR\n");
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm);

    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        // Your old code used error 3 for file errors
        Error::set(3);
        TextIO::print("?FILE NOT FOUND ERROR\n");
        return;
    }

    TextIO::print("LOADING " + filename + "\n");
    // Read the file byte-by-byte into program memory
    char c;
    uint16_t ptr = vm.PROGRAM_START_ADDR;
    while (infile.get(c) && ptr < vm.memory.size() - 2) {
        vm.memory[ptr++] = static_cast<uint8_t>(c);
    }

    // Write the end-of-program marker
    vm.memory[ptr++] = static_cast<uint8_t>(Tokens::ID::NOCMD);
    vm.memory[ptr] = 0;
}

void Commands::do_save(NeReLaBasic& vm) {
    if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) != Tokens::ID::STRING) {
        Error::set(1);
        TextIO::print("?SYNTAX ERROR\n");
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm);

    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        Error::set(3);
        TextIO::print("?FILE I/O ERROR\n");
        return;
    }

    TextIO::print("SAVING " + filename + "\n");
    // Write program memory to the file until we hit the end marker
    uint16_t ptr = vm.PROGRAM_START_ADDR;
    while (true) {
        uint8_t token = vm.memory[ptr];
        if (token == static_cast<uint8_t>(Tokens::ID::NOCMD) && vm.memory[ptr + 1] == 0) {
            break;
        }
        outfile.put(static_cast<char>(token));
        ptr++;
        if (ptr >= vm.memory.size() - 1) break;
    }
}

void Commands::do_run(NeReLaBasic& vm) {
    vm.run_program();
}
