// Commands.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include "Commands.hpp"
#include "NeReLaBasic.hpp" // We need the full class definition here
#include "TextIO.hpp"
#include "Tokens.hpp"
#include "Error.hpp"
#include "Types.hpp"


namespace { // Use an anonymous namespace to keep this helper local to this file
    // Forward declaration of the recursive part
    std::string array_to_string_recursive(
        const Array& arr,
        size_t& data_index, // Pass as a reference to be advanced
        size_t current_dimension
    );

    std::string value_to_string_for_array(const BasicValue& val) {
        // A specialized to_string to avoid infinite recursion if an array contains itself
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<Array>>) {
                return "<Array>"; // Don't recursively print nested arrays
            }
            else {
                return to_string(arg); // Use the main to_string for all other types
            }
            }, val);
    }

    std::string array_to_string_recursive(const Array& arr, size_t& data_index, size_t current_dimension) {
        std::stringstream ss;
        ss << "[";

        bool is_innermost_vector = (current_dimension == arr.shape.size() - 1);

        for (size_t i = 0; i < arr.shape[current_dimension]; ++i) {
            if (is_innermost_vector) {
                if (data_index < arr.data.size()) {
                    ss << value_to_string_for_array(arr.data[data_index++]);
                }
            }
            else {
                ss << array_to_string_recursive(arr, data_index, current_dimension + 1);
            }

            if (i < arr.shape[current_dimension] - 1) {
                ss << " "; // Separator between elements/sub-arrays
            }
        }
        ss << "]";
        return ss.str();
    }
} // end anonymous namespace

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return s;
}

// Helper to read a null-terminated string from p_code memory
std::string read_string(NeReLaBasic& vm) {
    std::string s;
    while ((*vm.active_p_code)[vm.pcode] != 0) {
        s += (*vm.active_p_code)[vm.pcode++];
    }
    vm.pcode++; // Skip the null terminator
    return s;
}


// Finds a variable by walking the call stack backwards, then checking globals.
BasicValue& get_variable(NeReLaBasic& vm, const std::string& name) {
    // 1. Search backwards through the call stack for the variable.
    if (!vm.call_stack.empty()) {
        for (auto it = vm.call_stack.rbegin(); it != vm.call_stack.rend(); ++it) {
            if (it->local_variables.count(name)) {
                return it->local_variables.at(name);
            }
        }
    }

    // 2. If not found in any local scope, check the global scope.
    // Note: Using .at() would throw an error if the key doesn't exist.
    // Using operator[] will create it if it doesn't exist, which is the
    // desired behavior for BASIC (e.g., undeclared variables default to 0).
    return vm.variables[name];
}

// Sets a variable. If inside a function, it sets the variable in the
// CURRENT function's local scope. Otherwise, it sets a global variable.
void set_variable(NeReLaBasic& vm, const std::string& name, const BasicValue& value) {
    // If we are inside a function/subroutine...
    if (!vm.call_stack.empty()) {
        // First, check if a LOCAL variable with this name already exists in the current scope.
        // This is important for loops or multiple assignments to the same local variable.
        if (vm.call_stack.back().local_variables.count(name)) {
            vm.call_stack.back().local_variables[name] = value;
            return;
        }

        // Second, check if a GLOBAL variable with this name exists.
        // If so, update the global one INSTEAD of creating a new local one.
        if (vm.variables.count(name)) {
            vm.variables[name] = value;
            return;
        }

        // Finally, if it's not in the local scope and not in the global scope,
        // it is a brand new variable, which we will define as local to the subroutine.
        vm.call_stack.back().local_variables[name] = value;

    }
    else {
        // Not in a subroutine, so it must be a global variable.
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
        else if constexpr (std::is_same_v<T, int>) {
            std::string s = std::to_string(arg);
            return s;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        }
        else if constexpr (std::is_same_v<T, FunctionRef>) {
            // Return a descriptive string for the function reference.
            return "<Function: " + arg.name + ">";
        }
        else if constexpr (std::is_same_v<T, DateTime>) {
            // Logic to convert DateTime to a string
            auto tp = arg.time_point;
            auto in_time_t = std::chrono::system_clock::to_time_t(tp);
            std::stringstream ss;
#pragma warning(suppress : 4996)
            ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<Array>>) {
            if (!arg) {
                return "<Null Array>";
            }
            if (arg->shape.empty() || arg->data.empty()) {
                return "[]"; // An empty array
            }
            size_t data_idx = 0;
            return array_to_string_recursive(*arg, data_idx, 0);
        }
        }, val);
}

void print_value(const BasicValue& val) {
    // For arrays, we just want to print their string representation.
    // For all other types, we can use the existing logic.
    if (std::holds_alternative<std::shared_ptr<Array>>(val)) {
        TextIO::print(to_string(val));
        return;
    }

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
        else if constexpr (std::is_same_v<T, int>) {
            // Convert int to string for printing
            std::string s = std::to_string(arg);
            TextIO::print(s);
        }
        else if constexpr (std::is_same_v<T, DateTime>) {
            // Logic to print DateTime
            auto tp = arg.time_point;
            auto in_time_t = std::chrono::system_clock::to_time_t(tp);
            std::stringstream ss;
#pragma warning(suppress : 4996)
            ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
            TextIO::print(ss.str());
        }
        else if constexpr (std::is_same_v<T, std::string>) { 
            TextIO::print(arg);
        }
        }, val);
}

void dump_p_code(const std::vector<uint8_t>& p_code_to_dump, const std::string& name) {
    const int bytes_per_line = 16;
    TextIO::print("Dumping p_code for '" + name + "' (" + std::to_string(p_code_to_dump.size()) + " bytes):\n");

    for (size_t i = 0; i < p_code_to_dump.size(); i += bytes_per_line) {
        // Print Address
        std::stringstream ss_addr;
        ss_addr << "0x" << std::setw(4) << std::setfill('0') << std::hex << i;
        TextIO::print(ss_addr.str() + " : ");

        // Print Hex Bytes
        std::stringstream ss_hex;
        for (int j = 0; j < bytes_per_line; ++j) {
            if (i + j < p_code_to_dump.size()) {
                ss_hex << std::setw(2) << std::setfill('0') << std::hex
                    << static_cast<int>(p_code_to_dump[i + j]) << " ";
            }
            else {
                ss_hex << "   ";
            }
        }
        TextIO::print(ss_hex.str() + " : ");

        // Print ASCII characters
        std::stringstream ss_ascii;
        for (int j = 0; j < bytes_per_line; ++j) {
            if (i + j < p_code_to_dump.size()) {
                char c = p_code_to_dump[i + j];
                if (isprint(static_cast<unsigned char>(c))) {
                    ss_ascii << c;
                }
                else {
                    ss_ascii << '.';
                }
            }
        }
        TextIO::print(ss_ascii.str());
        TextIO::nl();
    }
}


void Commands::do_dim(NeReLaBasic& vm) {
    Tokens::ID var_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]);
    std::string var_name = to_upper(read_string(vm));

    Tokens::ID next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);

    if (next_token == Tokens::ID::C_LEFTBRACKET) {
        // --- Case 1: ARRAY DECLARATION (e.g., DIM A[2, 3]) ---
        vm.pcode++; // Consume '['

        std::vector<size_t> dimensions;
        while (true) {
            BasicValue size_val = vm.evaluate_expression();
            if (Error::get() != 0) return;

            double dim_double = to_double(size_val);
            if (dim_double < 0) {
                Error::set(10, vm.runtime_current_line); // Bad subscript
                return;
            }
            dimensions.push_back(static_cast<size_t>(dim_double));

            Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (separator == Tokens::ID::C_COMMA) {
                vm.pcode++;
            }
            else if (separator == Tokens::ID::C_RIGHTBRACKET) {
                break;
            }
            else {
                Error::set(1, vm.runtime_current_line); // Syntax error: Expected , or ]
                return;
            }
        }
        if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_RIGHTBRACKET) {
            Error::set(1, vm.runtime_current_line); return;
        }

        // --- Create the Array on the heap using std::make_shared ---
        auto new_array_ptr = std::make_shared<Array>();
        new_array_ptr->shape = dimensions;

        size_t total_size = new_array_ptr->size();

        bool is_string_array = (var_name.back() == '$');
        BasicValue default_val = is_string_array ? BasicValue{ std::string("") } : BasicValue{ 0.0 };

        new_array_ptr->data.assign(total_size, default_val);

        // Store the shared_ptr in the BasicValue variant
        set_variable(vm, var_name, new_array_ptr);
    }
    else
    {
        // --- Case 2: TYPED VARIABLE DECLARATION (e.g., DIM V AS DATE) ---
        // This logic remains unchanged.
        if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::AS) {
            Error::set(1, vm.runtime_current_line); return;
        }

        Tokens::ID type_name_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]);
        if (type_name_token != Tokens::ID::VARIANT && type_name_token != Tokens::ID::INT) {
            Error::set(1, vm.runtime_current_line); return;
        }

        std::string type_name = to_upper(read_string(vm));
        DataType declared_type;

        if (type_name == "INTEGER") {
            declared_type = DataType::INTEGER;
        }
        else if (type_name == "DOUBLE") {
            declared_type = DataType::DOUBLE;
        }
        else if (type_name == "STRING") {
            declared_type = DataType::STRING;
        }
        else if (type_name == "DATE") {
            declared_type = DataType::DATETIME;
        }
        else if (type_name == "BOOLEAN" || type_name == "BOOL") {
            declared_type = DataType::BOOL;
        }
        else {
            Error::set(1, vm.runtime_current_line); // Syntax Error: Unknown type
            return;
        }

        vm.variable_types[var_name] = declared_type;

        BasicValue default_value;
        switch (declared_type) {
        case DataType::INTEGER:  default_value = 0; break;
        case DataType::DOUBLE:   default_value = 0.0; break;
        case DataType::STRING:   default_value = std::string(""); break;
        case DataType::BOOL:     default_value = false; break;
        case DataType::DATETIME: default_value = DateTime{}; break;
        default:                 default_value = 0.0; break;
        }
        set_variable(vm, var_name, default_value);
    }
}

void Commands::do_input(NeReLaBasic& vm) {
    // Peek at the next token to see if there is an optional prompt string.
    Tokens::ID next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);

    if (next_token == Tokens::ID::STRING) {
        // --- Case 1: Handle a prompt string ---
        BasicValue prompt = vm.evaluate_expression();
        if (Error::get() != 0) return;
        TextIO::print(to_string(prompt));

        // After the prompt, there MUST be a separator (',' or ';').
        Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
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
    next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
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
        Tokens::ID next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
        if (next_token == Tokens::ID::NOCMD || next_token == Tokens::ID::C_CR || next_token == Tokens::ID::C_COLON) {
            TextIO::nl(); return;
        }

        BasicValue result = vm.evaluate_expression();
        if (Error::get() != 0) {
            return; // Stop if the expression had an error
        }
        print_value(result); // Use our helper to print the result, whatever its type

        // --- Step 2: Look ahead for a separator ---
        Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);

        if (separator == Tokens::ID::C_COMMA) {
            vm.pcode++; // Consume the comma
            // Check if the line ends right after the comma
            Tokens::ID after_separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (after_separator == Tokens::ID::NOCMD || after_separator == Tokens::ID::C_CR || next_token == Tokens::ID::C_COLON) {
                return; // Ends with a comma, so no newline.
            }
            TextIO::print("\t"); // Otherwise, print a tab and continue the loop.
        }
        else if (separator == Tokens::ID::C_SEMICOLON) {
            vm.pcode++; // Consume the semicolon
            // Check if the line ends right after the semicolon
            Tokens::ID after_separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (after_separator == Tokens::ID::NOCMD || after_separator == Tokens::ID::C_CR || next_token == Tokens::ID::C_COLON) {
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
    Tokens::ID var_type_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
    vm.pcode++;
    std::string name = to_upper(read_string(vm));

    // --- Case 1: ARRAY ELEMENT ASSIGNMENT (e.g., A[i, j] = ...) ---
    if (var_type_token == Tokens::ID::ARRAY_ACCESS) {
        if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_LEFTBRACKET) {
            Error::set(1, vm.runtime_current_line); return;
        }

        // Parse multiple comma-separated indices
        std::vector<size_t> indices;
        while (true) {
            BasicValue index_val = vm.evaluate_expression();
            if (Error::get() != 0) return;
            indices.push_back(static_cast<size_t>(to_double(index_val)));

            Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (separator == Tokens::ID::C_RIGHTBRACKET) break;
            if (separator != Tokens::ID::C_COMMA) { Error::set(1, vm.runtime_current_line); return; }
            vm.pcode++;
        }
        vm.pcode++; // Consume ']'

        if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_EQ) {
            Error::set(1, vm.runtime_current_line); return;
        }
        BasicValue value_to_assign = vm.evaluate_expression();
        if (Error::get() != 0) return;

        // Get the array, check its type, and perform assignment
        BasicValue& array_var = get_variable(vm, name);
        if (!std::holds_alternative<std::shared_ptr<Array>>(array_var)) {
            Error::set(15, vm.runtime_current_line); // Type Mismatch
            return;
        }
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(array_var);
        if (!arr_ptr) { Error::set(15, vm.runtime_current_line); return; }

        try {
            size_t flat_index = arr_ptr->get_flat_index(indices);
            arr_ptr->data[flat_index] = value_to_assign;
        }
        catch (const std::exception&) {
            Error::set(10, vm.runtime_current_line); // Bad subscript
        }
    }
    // --- Case 2: WHOLE VARIABLE ASSIGNMENT (e.g., A = ...) ---
    else {
        if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_EQ) {
            Error::set(1, vm.runtime_current_line); return;
        }

        // The universal evaluate_expression now handles all cases,
        // including array literals, thanks to our changes in parse_primary.
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
        Error::set(11, vm.runtime_current_line); // Use a new error code
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
        uint8_t lsb = (*vm.active_p_code)[jump_placeholder_addr];
        uint8_t msb = (*vm.active_p_code)[jump_placeholder_addr + 1];
        vm.pcode = (msb << 8) | lsb;
    }
    // If true, we do nothing and just continue execution from the current pcode.
}

// Correct do_else implementation
void Commands::do_else(NeReLaBasic& vm) {
    // ELSE is an unconditional jump. The placeholder is right after the token.
    uint16_t jump_placeholder_addr = vm.pcode;
    uint8_t lsb = (*vm.active_p_code)[jump_placeholder_addr];
    uint8_t msb = (*vm.active_p_code)[jump_placeholder_addr + 1];
    vm.pcode = (msb << 8) | lsb;
}

void Commands::do_for(NeReLaBasic& vm) {
    // FOR [variable] = [start_expr] TO [end_expr] STEP [step_expr]

    // 1. Get the loop variable name.
    Tokens::ID var_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
    if (var_token != Tokens::ID::VARIANT && var_token != Tokens::ID::INT) {
        Error::set(1, vm.runtime_current_line); // Syntax Error
        return;
    }
    vm.pcode++;
    std::string var_name = read_string(vm);

    // 2. Expect an equals sign.
    if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_EQ) {
        Error::set(1, vm.runtime_current_line);
        return;
    }

    // 3. Evaluate the start expression and assign it.
    BasicValue start_val = vm.evaluate_expression();
    if (Error::get() != 0) return;
    set_variable(vm, var_name, start_val);
    //vm.variables[var_name] = start_val;

    // 4. The 'TO' keyword was skipped by the tokenizer. Evaluate the end expression.
    BasicValue end_val = vm.evaluate_expression();
    if (Error::get() != 0) return;

    // 5. --- THIS IS THE CORRECTED LOGIC FOR STEP ---
    double step_val = 1.0; // Default step is 1.

    // The TO and STEP keywords were skipped by the tokenizer.
    // If we are NOT at the end of the line, what's left must be the step expression.
    Tokens::ID next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
    if (next_token != Tokens::ID::C_CR && next_token != Tokens::ID::NOCMD && next_token != Tokens::ID::C_COLON)
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
    double current_val = to_double(get_variable(vm, current_loop.variable_name));
    current_val += current_loop.step_value;
    set_variable(vm, current_loop.variable_name, current_val);
    //vm.variables[current_loop.variable_name] = current_val;

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
    uint8_t lsb = (*vm.active_p_code)[vm.pcode++];
    uint8_t msb = (*vm.active_p_code)[vm.pcode++];
    uint16_t jump_over_address = (msb << 8) | lsb;

    // Set pcode to the target, skipping the entire function body.
    vm.pcode = jump_over_address;
}

void Commands::do_callfunc(NeReLaBasic& vm) {
    std::string identifier_being_called = to_upper(read_string(vm));
    std::string real_func_to_call = identifier_being_called;

    if (!vm.active_function_table->count(real_func_to_call)) {
        BasicValue& var = get_variable(vm, identifier_being_called);
        if (std::holds_alternative<FunctionRef>(var)) {
            real_func_to_call = std::get<FunctionRef>(var).name;
        }
    }

    if (!vm.active_function_table->count(real_func_to_call)) {
        Error::set(22, vm.runtime_current_line); return;
    }

    const auto& func_info = vm.active_function_table->at(real_func_to_call);
    std::vector<BasicValue> args;

    // --- CORRECTED, SIMPLIFIED ARGUMENT PARSING ---
    if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode++]) != Tokens::ID::C_LEFTPAREN) {
        Error::set(1, vm.runtime_current_line); return;
    }
    if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]) != Tokens::ID::C_RIGHTPAREN) {
        while (true) {
            args.push_back(vm.evaluate_expression());
            if (Error::get() != 0) return;
            Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (separator == Tokens::ID::C_RIGHTPAREN) break;
            if (separator != Tokens::ID::C_COMMA) { Error::set(1, vm.runtime_current_line); return; }
            vm.pcode++;
        }
    }
    vm.pcode++; // Consume ')'

    if (func_info.arity != -1 && args.size() != func_info.arity) {
        Error::set(26, vm.runtime_current_line); return;
    }

    // --- Execution Logic ---
    if (func_info.native_impl != nullptr) {
        func_info.native_impl(vm, args);
    }
    else {
        NeReLaBasic::StackFrame frame;
        // It's a user-defined BASIC function. Set up the stack frame.
        for (size_t i = 0; i < func_info.parameter_names.size(); ++i) {
            if (i < args.size()) {
                frame.local_variables[func_info.parameter_names[i]] = args[i];
            }
        }


        frame.return_p_code_ptr = vm.active_p_code;
        frame.return_pcode = vm.pcode;
        // NEW: Save the entire function table of the caller
        frame.previous_function_table_ptr = vm.active_function_table;
        //frame.for_stack_size_on_entry = vm.for_stack.size();
        vm.call_stack.push_back(frame);

        // --- CONTEXT SWITCH ---
        if (!func_info.module_name.empty() && vm.compiled_modules.count(func_info.module_name)) {
            auto& target_module = vm.compiled_modules[func_info.module_name];
            vm.active_p_code = &target_module.p_code;
            vm.active_function_table = &target_module.function_table;
        }

        vm.pcode = func_info.start_pcode;
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
    vm.for_stack.resize(frame.for_stack_size_on_entry);
    vm.active_p_code = frame.return_p_code_ptr; // Restore bytecode context
    vm.pcode = frame.return_pcode;              // Restore program counter
    vm.active_function_table = frame.previous_function_table_ptr;
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
    vm.for_stack.resize(frame.for_stack_size_on_entry);
    vm.active_p_code = frame.return_p_code_ptr; // Restore bytecode context
    vm.pcode = frame.return_pcode;              // Restore program counter
    vm.active_function_table = frame.previous_function_table_ptr;
    vm.call_stack.pop_back();
}


// At runtime, SUB just jumps over the procedure body.
// This is identical to how FUNC works.
void Commands::do_sub(NeReLaBasic& vm) {
    Commands::do_func(vm);
}

void Commands::do_callsub(NeReLaBasic& vm) {
    std::string proc_name = to_upper(read_string(vm));

    if (!vm.active_function_table->count(proc_name)) {
        Error::set(22, vm.runtime_current_line); return;
    }
    const auto& proc_info = vm.active_function_table->at(proc_name);
    std::vector<BasicValue> args;

    Tokens::ID token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);

    if (token != Tokens::ID::C_CR) {
        while (true) {
            args.push_back(vm.evaluate_expression());
            if (Error::get() != 0) return;
            Tokens::ID separator = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);
            if (separator == Tokens::ID::C_CR) break;
            if (separator != Tokens::ID::C_COMMA) { Error::set(1, vm.runtime_current_line); return; }
            vm.pcode++;
        }
    }

    if (proc_info.arity != -1 && args.size() != proc_info.arity) {
        Error::set(26, vm.runtime_current_line); return;
    }

    if (proc_info.native_impl != nullptr) {
        proc_info.native_impl(vm, args);
    }
    else {
        NeReLaBasic::StackFrame frame;
        frame.return_p_code_ptr = vm.active_p_code;
        frame.return_pcode = vm.pcode;
        frame.previous_function_table_ptr = vm.active_function_table;
        frame.for_stack_size_on_entry = vm.for_stack.size();
        for (size_t i = 0; i < proc_info.parameter_names.size(); ++i) {
            if (i < args.size()) frame.local_variables[proc_info.parameter_names[i]] = args[i];
        }
        vm.call_stack.push_back(frame);

        if (!proc_info.module_name.empty() && vm.compiled_modules.count(proc_info.module_name)) {
            auto& target_module = vm.compiled_modules[proc_info.module_name];
            vm.active_p_code = &target_module.p_code;
            vm.active_function_table = &target_module.function_table;
        }
        vm.pcode = proc_info.start_pcode;
    }
}

// At runtime, ENDSUB handles returning from a procedure call.
// It pops the call stack and restores the program counter.
void Commands::do_endsub(NeReLaBasic& vm) {
    if (vm.call_stack.empty()) {
        Error::set(9, vm.runtime_current_line); // RETURN without GOSUB/CALL
        return;
    }
    // For a procedure, there is no return value to set.
    // We just pop the stack and set pcode to the return address.
    auto& frame = vm.call_stack.back();
    vm.for_stack.resize(frame.for_stack_size_on_entry);
    vm.active_p_code = frame.return_p_code_ptr; // Restore context
    vm.pcode = frame.return_pcode;              // Restore PC
    vm.active_function_table = frame.previous_function_table_ptr;
    vm.call_stack.pop_back();
}

void Commands::do_list(NeReLaBasic& vm) {
    // Simply print the stored source code.
    TextIO::print(vm.source_code);
}

void Commands::do_load(NeReLaBasic& vm) {
    // LOAD expects a filename
    if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]) != Tokens::ID::STRING) {
        Error::set(1, vm.runtime_current_line);
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm); // read_string now reads from (*vm.active_p_code)

    std::ifstream infile(filename); // Open in text mode
    if (!infile) {
        Error::set(6, vm.runtime_current_line);
        return;
    }

    TextIO::print("LOADING " + filename + "\n");
    // Read the entire file into the source_code string
    std::stringstream buffer;
    buffer << infile.rdbuf();
    vm.source_code = buffer.str();
}

void Commands::do_save(NeReLaBasic& vm) {
    if (static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]) != Tokens::ID::STRING) {
        Error::set(1, vm.runtime_current_line);
        return;
    }
    vm.pcode++; // Consume STRING token
    std::string filename = read_string(vm); // read_string now reads from (*vm.active_p_code)

    std::ofstream outfile(filename); // Open in text mode
    if (!outfile) {
        Error::set(12, vm.runtime_current_line);
        return;
    }

    TextIO::print("SAVING " + filename + "\n");
    // Write the source_code string directly to the file
    outfile << vm.source_code;
}

void Commands::do_compile(NeReLaBasic& vm) {
    // Compile into the main program buffer
    TextIO::print("Compiling...\n");
    if (vm.tokenize_program(vm.program_p_code, vm.source_code) == 0) {
        if (!vm.if_stack.empty()) {
            // There are unclosed IF blocks. Get the line number of the last one.
            uint16_t error_line = vm.if_stack.back().source_line;
            Error::set(4, error_line); // New Error: Missing ENDIF
        }
        else {
            TextIO::print("OK. Program compiled to " + std::to_string(vm.program_p_code.size()) + " bytes.\n");
        }
    }
    else {
        // Error message is printed by tokenize_program
        TextIO::print("Compilation failed.\n");
    }
}

void Commands::do_stop(NeReLaBasic& vm) {
    TextIO::print("\nBreak in line " + std::to_string(vm.runtime_current_line) + "\n");
    vm.is_stopped = true;
}

void Commands::do_run(NeReLaBasic& vm) {

    do_compile(vm);
    if (Error::get() != 0) {
        Error::print();
    }

    // Clear variables and prepare for a clean run
    vm.variables.clear();
    vm.call_stack.clear();
    vm.for_stack.clear();
    Error::clear();
    vm.is_stopped = false; // Reset the stopped state


    vm.active_function_table = &vm.main_function_table;

    TextIO::print("Running...\n");
    // Execute from the main program buffer
    vm.execute(vm.program_p_code);

    // If the execution resulted in an error, print it
    if (Error::get() != 0) {
        Error::print();
    }
    vm.active_function_table = &vm.main_function_table;
}

void Commands::do_tron(NeReLaBasic& vm) {
    vm.trace = 1;
    TextIO::print("TRACE ON\n");
}

void Commands::do_troff(NeReLaBasic& vm) {
    vm.trace = 0;
    TextIO::print("TRACE OFF\n");
}

void Commands::do_dump(NeReLaBasic& vm) {
    // Peek at the next token to see if an argument was provided.
    Tokens::ID next_token = static_cast<Tokens::ID>((*vm.active_p_code)[vm.pcode]);

    if (next_token == Tokens::ID::NOCMD || next_token == Tokens::ID::C_CR) {
        // Case 1: No argument provided, dump the main program p-code.
        dump_p_code(vm.program_p_code, "main program");
        return;
    }

    // An argument was provided. Evaluate it as an expression.
    // This will handle `DUMP GLOBAL` where GLOBAL is parsed as a variable/identifier.
    BasicValue arg_val = vm.evaluate_expression();
    if (Error::get() != 0) return;

    std::string arg_str = to_upper(to_string(arg_val));

    if (arg_str == "GLOBAL") {
        TextIO::print("--- Global Variables ---\n");
        if (vm.variables.empty()) {
            TextIO::print("(No global variables defined)\n");
        }
        else {
            for (const auto& pair : vm.variables) {
                TextIO::print(pair.first + " = " + to_string(pair.second) + "\n");
            }
        }
    }
    else if (arg_str == "LOCAL") {
        TextIO::print("--- Local Variables ---\n");
        if (vm.call_stack.empty()) {
            TextIO::print("(Not inside a function/subroutine)\n");
        }
        else {
            const auto& locals = vm.call_stack.back().local_variables;
            if (locals.empty()) {
                TextIO::print("(No local variables in current scope)\n");
            }
            else {
                for (const auto& pair : locals) {
                    TextIO::print(pair.first + " = " + to_string(pair.second) + "\n");
                }
            }
        }
    }
    else {
        // Fallback to original behavior: dump p-code for a module.
        if (vm.compiled_modules.count(arg_str)) {
            dump_p_code(vm.compiled_modules.at(arg_str).p_code, arg_str);
        }
        else {
            TextIO::print("? Error: Module '" + arg_str + "' not found, or invalid DUMP argument.\n");
        }
    }
}
