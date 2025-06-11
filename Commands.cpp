// Commands.cpp
#include <fstream> 
#include <algorithm>
#include "Commands.hpp"
#include "NeReLaBasic.hpp" // We need the full class definition here
#include "TextIO.hpp"
#include "Tokens.hpp"
#include "Error.hpp"
#include "Types.hpp"
#include <iostream>

//namespace {
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
//}

// Finds a variable, checking local scope first, then global.
// Returns a reference so it can be read from or assigned to.
BasicValue& get_variable(NeReLaBasic& vm, const std::string& name) {
    // Search in the innermost local scope (the top of the call stack).
    if (!vm.call_stack.empty() && vm.call_stack.back().local_variables.count(name)) {
        return vm.call_stack.back().local_variables.at(name);
    }
    // Otherwise, fall back to the global scope.
    return vm.variables[name];
}

// Sets a variable, creating it as a local if we're inside a function.
void set_variable(NeReLaBasic& vm, const std::string& name, const BasicValue& value) {
    // If we are in a function call, all assignments create/update local variables.
    if (!vm.call_stack.empty()) {
        vm.call_stack.back().local_variables[name] = value;
    }
    else {
        // Otherwise, it's a global variable.
        vm.variables[name] = value;
    }
}

std::string to_string(const BasicValue& val) {
    // std::visit will execute the correct lambda block based on the type currently held in val
    return std::visit([](auto&& arg) -> std::string {
        // Get the actual type of the argument
        using T = std::decay_t<decltype(arg)>;

        // Use 'if constexpr' to handle each possible type in the variant
        if constexpr (std::is_same_v<T, bool>) {
            return arg ? "TRUE" : "FALSE";
        }
        else if constexpr (std::is_same_v<T, double>) {
            std::string s = std::to_string(arg);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') { s.pop_back(); }
            return s;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        }
        else if constexpr (std::is_same_v<T, FunctionRef>) {
            // --- THIS IS THE NEW, MISSING LOGIC ---
            // Return a descriptive string for the function reference.
            return "<Function: " + arg.name + ">";
        }
        }, val);
}

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return s;
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
        else if constexpr (std::is_same_v<T, std::string>) { 
            TextIO::print(arg);
        }
        }, val);
}

void Commands::do_dim(NeReLaBasic& vm) {
    Tokens::ID array_token = static_cast<Tokens::ID>(vm.memory[vm.pcode++]);
    std::string array_name = to_upper(read_string(vm));

    //// Expect an opening bracket '['
    //if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_LEFTBRACKET) {
    //    Error::set(1, vm.runtime_current_line); return;
    //}

    BasicValue size_val = vm.evaluate_expression();
    if (Error::get() != 0) return;
    int size = static_cast<int>(to_double(size_val));

    // Expect a closing bracket ']'
    if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
        Error::set(1, vm.runtime_current_line); return;
    }

    bool is_string_array = (array_name.back() == '$');
    BasicValue default_val = is_string_array ? BasicValue{ std::string("") } : BasicValue{ 0.0 };
    vm.arrays[array_name] = std::vector<BasicValue>(size + 1, default_val);
}

void Commands::do_input(NeReLaBasic& vm) {
    // Peek at the next token to see if there is an optional prompt string.
    Tokens::ID next_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);

    if (next_token == Tokens::ID::STRING) {
        // --- Case 1: Handle a prompt string ---
        BasicValue prompt = vm.evaluate_expression();
        if (Error::get() != 0) return;
        TextIO::print(to_string(prompt));

        // After the prompt, there MUST be a separator (',' or ';').
        Tokens::ID separator = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
        if (separator == Tokens::ID::C_SEMICOLON) {
            vm.pcode++; // Consume the semicolon
            TextIO::print(" "); // Suppress the '?' and print a space
        }
        else if (separator == Tokens::ID::C_COMMA) {
            vm.pcode++; // Consume the comma
            TextIO::print("? "); // Print the '?'
        }
        else {
            // No separator after the prompt is a syntax error
            Error::set(1, vm.runtime_current_line);
            return;
        }
    }
    else {
        // --- Case 2: No prompt string ---
        TextIO::print("? ");
    }

    // Now, we are past the prompt and separator, so we can get the variable name.
    next_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
    if (next_token != Tokens::ID::VARIANT && next_token != Tokens::ID::INT && next_token != Tokens::ID::STRVAR) {
        Error::set(1, vm.runtime_current_line); // Syntax error, expected a variable
        return;
    }
    vm.pcode++;
    std::string var_name = to_upper(read_string(vm));

    // Read a full line of input from the user.
    std::string user_input_line;
    std::getline(std::cin, user_input_line);

    // Store the value, converting type if necessary.
    if (var_name.back() == '$') {
        // It's a string variable, do a direct assignment.
        set_variable(vm, var_name, user_input_line);
    }
    else {
        // It's a numeric variable. Try to convert the input to a double.
        try {
            double num_val = std::stod(user_input_line);
            set_variable(vm, var_name, num_val);
        }
        catch (const std::exception&) {
            set_variable(vm, var_name, 0.0);
        }
    }
}
void Commands::do_print(NeReLaBasic& vm) {
    // Loop through all items in the PRINT list until the statement ends.
    while (true) {
        // Peek at the next token to decide what to do.
        Tokens::ID next_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
        if (next_token == Tokens::ID::NOCMD || next_token == Tokens::ID::C_CR) {
            TextIO::nl(); return;
        }

        BasicValue result = vm.evaluate_expression();
        if (Error::get() != 0) {
            return; // Stop if the expression had an error
        }
        print_value(result); // Use our helper to print the result, whatever its type

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
//void Commands::do_let(NeReLaBasic& vm) {
//    std::string var_name;
//    while (vm.memory[vm.pcode] != 0) { var_name += vm.memory[vm.pcode++]; }
//    vm.pcode++;
//    if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) != Tokens::ID::C_EQ) {
//        Error::set(1, vm.runtime_current_line); return;
//    }
//    vm.pcode++;
//    BasicValue value = vm.evaluate_expression(); // This now returns a BasicValue
//    if (Error::get() != 0) return;
//    set_variable(vm, var_name, value); // Store the BasicValue in the new map
//}

void Commands::do_let(NeReLaBasic& vm) {
    Tokens::ID var_type_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
    vm.pcode++;
    std::string name = to_upper(read_string(vm));

    // Check if this is an array assignment, e.g., A[i] = ...
    if (var_type_token == Tokens::ID::ARRAY_ACCESS) {
        //if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_LEFTBRACKET) {
        //    Error::set(1, vm.runtime_current_line); return;
        //}
        int index = static_cast<int>(to_double(vm.evaluate_expression()));
        if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
            Error::set(1, vm.runtime_current_line); return;
        }
        if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_EQ) {
            Error::set(1, vm.runtime_current_line); return;
        }
        BasicValue value_to_assign = vm.evaluate_expression();
        if (Error::get() != 0) return;

        if (vm.arrays.count(name) && index >= 0 && index < vm.arrays.at(name).size()) {
            vm.arrays.at(name)[index] = value_to_assign;
        }
        else {
            Error::set(24, vm.runtime_current_line); // Bad subscript
        }
    }
    else { // It's a simple variable assignment, e.g., A = ...
        if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_EQ) {
            Error::set(1, vm.runtime_current_line); return;
        }
        BasicValue value_to_assign = vm.evaluate_expression();
        if (Error::get() != 0) return;
        set_variable(vm, name, value_to_assign);
    }
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
        Error::set(20, vm.runtime_current_line); // Use a new error code
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

void Commands::do_for(NeReLaBasic& vm) {
    // FOR [variable] = [start_expr] TO [end_expr] STEP [step_expr]

    // 1. Get the loop variable name.
    Tokens::ID var_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
    if (var_token != Tokens::ID::VARIANT && var_token != Tokens::ID::INT) {
        Error::set(1, vm.runtime_current_line); // Syntax Error
        return;
    }
    vm.pcode++;
    std::string var_name = read_string(vm);

    // 2. Expect an equals sign.
    if (static_cast<Tokens::ID>(vm.memory[vm.pcode++]) != Tokens::ID::C_EQ) {
        Error::set(1, vm.runtime_current_line);
        return;
    }

    // 3. Evaluate the start expression and assign it.
    BasicValue start_val = vm.evaluate_expression();
    if (Error::get() != 0) return;
    vm.variables[var_name] = start_val;

    // 4. The 'TO' keyword was skipped by the tokenizer. Evaluate the end expression.
    BasicValue end_val = vm.evaluate_expression();
    if (Error::get() != 0) return;

    // 5. --- THIS IS THE CORRECTED LOGIC FOR STEP ---
    double step_val = 1.0; // Default step is 1.

    // The TO and STEP keywords were skipped by the tokenizer.
    // If we are NOT at the end of the line, what's left must be the step expression.
    Tokens::ID next_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
    if (next_token != Tokens::ID::C_CR && next_token != Tokens::ID::NOCMD)
    {
        BasicValue step_expr_val = vm.evaluate_expression();
        if (Error::get() != 0) return;
        step_val = to_double(step_expr_val);
    }

    // 6. Push all the loop info onto the FOR stack.
    NeReLaBasic::ForLoopInfo loop_info;
    loop_info.variable_name = var_name;
    loop_info.end_value = to_double(end_val);
    loop_info.step_value = step_val;
    loop_info.loop_start_pcode = vm.pcode;

    vm.for_stack.push_back(loop_info);
}
void Commands::do_next(NeReLaBasic& vm) {
    // 1. Check if there is anything on the FOR stack.
    if (vm.for_stack.empty()) {
        Error::set(21, vm.runtime_current_line); // New Error: NEXT without FOR
        return;
    }

    // 2. Get the info for the current (innermost) loop.
    NeReLaBasic::ForLoopInfo& current_loop = vm.for_stack.back();

    // 3. Get the loop variable's current value and increment it by the step.
    double current_val = to_double(vm.variables[current_loop.variable_name]);
    current_val += current_loop.step_value;
    vm.variables[current_loop.variable_name] = current_val;

    // 4. Check if the loop is finished.
    bool loop_finished = false;
    if (current_loop.step_value > 0) { // Positive step
        if (current_val > current_loop.end_value) {
            loop_finished = true;
        }
    }
    else { // Negative step
        if (current_val < current_loop.end_value) {
            loop_finished = true;
        }
    }

    // 5. Act on the check.
    if (loop_finished) {
        // The loop is over, pop it from the stack and continue execution.
        vm.for_stack.pop_back();
    }
    else {
        // The loop continues. Jump pcode back to the start of the loop.
        vm.pcode = current_loop.loop_start_pcode;
    }
}

void Commands::do_func(NeReLaBasic& vm) {
    // The FUNC token was consumed by statement(). pcode points to its arguments.
    // In our bytecode, the argument is the 2-byte address to jump to.
    uint8_t lsb = vm.memory[vm.pcode++];
    uint8_t msb = vm.memory[vm.pcode++];
    uint16_t jump_over_address = (msb << 8) | lsb;

    // Set pcode to the target, skipping the entire function body.
    vm.pcode = jump_over_address;
}

void Commands::do_callfunc(NeReLaBasic& vm) {
    // This handles a function call that is a statement, like `luli()`.
    // The CALLFUNC token was already consumed by statement(). pcode points to the name.

    std::string identifier_being_called = read_string(vm);
    std::string real_func_to_call = to_upper(identifier_being_called);

    // This logic is identical to the one in parse_primary
    if (!vm.function_table.count(real_func_to_call)) {
        if (get_variable(vm, identifier_being_called).index() == 3) { // 3 is FunctionRef
            real_func_to_call = std::get<FunctionRef>(get_variable(vm, identifier_being_called)).name;
        }
    }

    if (!vm.function_table.count(real_func_to_call)) {
        Error::set(22, vm.runtime_current_line);
        return;
    }

    const auto& func_info = vm.function_table.at(real_func_to_call);

    // The rest of this is a near-copy of the logic from parse_primary...
    if (func_info.native_impl != nullptr) {
        std::vector<BasicValue> args;
        vm.pcode++; // Skip '('
        bool first_arg = true;
        while (vm.pcode < vm.memory.size() && static_cast<Tokens::ID>(vm.memory[vm.pcode]) != Tokens::ID::C_RIGHTPAREN) {
            if (!first_arg) {
                if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) == Tokens::ID::C_COMMA) vm.pcode++;
                else { Error::set(1, vm.runtime_current_line); return; }
            }
            args.push_back(vm.evaluate_expression());
            if (Error::get() != 0) return;
            first_arg = false;
        }
        vm.pcode++; // Skip ')'
        if (func_info.arity != -1 && args.size() != func_info.arity) {
            Error::set(26, vm.runtime_current_line); return;
        }
        // Call the native function but IGNORE the return value
        func_info.native_impl(args);
    }
    else { // It's a user-defined BASIC function
        NeReLaBasic::StackFrame frame;
        vm.pcode++; // Skip '('
        for (const auto& param_name : func_info.parameter_names) {
            frame.local_variables[param_name] = vm.evaluate_expression();
            if (static_cast<Tokens::ID>(vm.memory[vm.pcode]) == Tokens::ID::C_COMMA) vm.pcode++;
        }
        vm.pcode++; // Skip ')'

        frame.return_pcode = vm.pcode;
        vm.call_stack.push_back(frame);
        vm.pcode = func_info.start_pcode;

        // Nested Execution Loop
        while (true) {
            Tokens::ID func_token = static_cast<Tokens::ID>(vm.memory[vm.pcode]);
            if (Error::get() != 0) break;
            if (func_token == Tokens::ID::ENDFUNC || func_token == Tokens::ID::RETURN) {
                vm.statement(); break;
            }
            vm.statement();
        }
        // IGNORE the return value stored in vm.variables["RETVAL"]
    }
}

// --- Implementation of do_return ---
void Commands::do_return(NeReLaBasic& vm) {
    if (vm.call_stack.empty()) {
        Error::set(23, vm.runtime_current_line); return;
    }

    // Evaluate the expression that comes after the RETURN keyword.
    BasicValue return_value = vm.evaluate_expression();
    if (Error::get() != 0) return;

    // Set the special return value variable.
    vm.variables["RETVAL"] = return_value;

    // Pop the stack and set pcode to the return address.
    auto& frame = vm.call_stack.back();
    vm.pcode = frame.return_pcode;
    vm.call_stack.pop_back();
}

void Commands::do_endfunc(NeReLaBasic& vm) {
    if (vm.call_stack.empty()) {
        Error::set(23, vm.runtime_current_line); return;
    }

    // ENDFUNC implies a default return value of 0.
    vm.variables["RETVAL"] = 0.0;

    // Pop the stack and set pcode to the return address.
    auto& frame = vm.call_stack.back();
    vm.pcode = frame.return_pcode;
    vm.call_stack.pop_back();
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
        Error::set(1, vm.runtime_current_line); // Syntax Error
        TextIO::print("?SYNTAX ERROR\n");
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm);

    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        // Your old code used error 3 for file errors
        Error::set(3, vm.runtime_current_line);
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
        Error::set(1, vm.runtime_current_line);
        TextIO::print("?SYNTAX ERROR\n");
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm);

    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        Error::set(3, vm.runtime_current_line);
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
