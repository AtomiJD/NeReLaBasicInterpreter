#pragma once
#include <variant>
#include <string>
#include <chrono>
#include <vector>     
#include <numeric>    // for std::accumulate
#include <stdexcept>  // for exceptions

// Forward-declare the Array struct so BasicValue can know it exists.
struct Array;

// An enum to represent the declared type of a variable.
enum class DataType {
    DEFAULT, // Original behavior, can hold any type
    BOOL,
    INTEGER,
    DOUBLE,
    STRING,
    DATETIME
};

// A struct to hold our date/time value, based on std::chrono.
struct DateTime {
    std::chrono::system_clock::time_point time_point;

    // Default constructor initializes to the current time.
    DateTime() : time_point(std::chrono::system_clock::now()) {}

    // Constructor from a time_point.
    DateTime(const std::chrono::system_clock::time_point& tp) : time_point(tp) {}

    bool operator==(const DateTime&) const = default;
};

// This struct represents a "pointer" or "reference" to a function.
// We just store the function's name.
struct FunctionRef {
    std::string name;
    // This lets std::variant compare it if needed.
    bool operator==(const FunctionRef&) const = default;
};

// --- Use a std::shared_ptr to break the circular dependency ---
using BasicValue = std::variant<bool, double, std::string, FunctionRef, int, DateTime, std::shared_ptr<Array>>;


// --- A structure to represent N-dimensional arrays ---
struct Array {
    std::vector<BasicValue> data; // The raw data, stored in a flat "raveled" format.
    std::vector<size_t> shape;    // The dimensions of the array. e.g., {5} for a vector, {2, 3} for a 2x3 matrix.

    // Default constructor for an empty array
    Array() = default;

    // A helper to calculate the total number of elements
    size_t size() const {
        if (shape.empty()) return 0;
        // Use std::accumulate to multiply all dimensions together
        return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
    }

    // Helper to get the index into the flat 'data' vector from dimensional indices
    size_t get_flat_index(const std::vector<size_t>& indices) const {
        if (indices.size() != shape.size()) {
            // This indicates a programmer error or a runtime error that should be caught
            throw std::runtime_error("Mismatched number of dimensions for indexing.");
        }
        size_t flat_index = 0;
        size_t multiplier = 1;
        for (int i = shape.size() - 1; i >= 0; --i) {
            if (indices[i] >= shape[i]) {
                // This is a runtime error (Index out of bounds)
                throw std::out_of_range("Array index out of bounds.");
            }
            flat_index += indices[i] * multiplier;
            multiplier *= shape[i];
        }
        return flat_index;
    }

    // This lets std::variant compare it if needed.
    bool operator==(const Array&) const = default;
};

//==============================================================================
// HELPER FUNCTIONS
// We define them here as 'inline' so they can be used across
// multiple .cpp files without causing linker errors.
//==============================================================================

// Helper to convert a BasicValue to a double for math operations.
// This is called "coercion". It treats booleans as 1.0 or 0.0.
inline double to_double(const BasicValue& val) {
    if (std::holds_alternative<double>(val)) {
        return std::get<double>(val);
    }
    if (std::holds_alternative<int>(val)) { 
        return static_cast<double>(std::get<int>(val));
    }
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? 1.0 : 0.0;
    }
    // Check if it holds a pointer to an array
    if (std::holds_alternative<std::shared_ptr<Array>>(val)) {
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(val);
        if (arr_ptr && arr_ptr->data.size() == 1) {
            // Coerce single-element array to a number
            return to_double(arr_ptr->data[0]);
        }
    }
    return 0.0;
}

// Helper to convert a BasicValue to a boolean for logic operations.
// It treats any non-zero number as true.
inline bool to_bool(const BasicValue& val) {
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val);
    }
    if (std::holds_alternative<double>(val)) {
        return std::get<double>(val) != 0.0;
    }
    // Check if it holds a pointer to an array
    if (std::holds_alternative<std::shared_ptr<Array>>(val)) {
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(val);
        if (arr_ptr && arr_ptr->data.size() == 1) {
            // Coerce single-element array to a bool
            return to_bool(arr_ptr->data[0]);
        }
    }
    return false;
}

