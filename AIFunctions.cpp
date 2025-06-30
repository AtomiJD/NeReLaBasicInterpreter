#include "AIFunctions.hpp"
#include "Error.hpp"
#include "Types.hpp"
#include "Commands.hpp" // For to_string, etc.

#include <random>
#include <functional>
#include <unordered_set>
#include <cmath>
#include <fstream>
#include <limits>
#include <algorithm>

// --- Helper Namespace for AI-specific utilities ---
/**
 * @brief Helper to sum a 2D array along a specific dimension (axis).
 * @param arr The 2D array to sum.
 * @param axis The axis to sum along (0 for columns, 1 for rows).
 * @return A new shared_ptr<Array> containing the summed vector.
 */
std::shared_ptr<Array> array_sum_along_axis(const std::shared_ptr<Array>& arr, int axis) {
    if (!arr || arr->shape.size() != 2) return nullptr;
    size_t rows = arr->shape[0];
    size_t cols = arr->shape[1];
    auto result_ptr = std::make_shared<Array>();

    if (axis == 0) { // Sum down the columns, result is a row vector
        result_ptr->shape = { 1, cols };
        result_ptr->data.assign(cols, 0.0);
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < rows; ++r) {
                result_ptr->data[c] = to_double(result_ptr->data[c]) + to_double(arr->data[r * cols + c]);
            }
        }
    }
    else if (axis == 1) { // Sum across the rows, result is a column vector
        result_ptr->shape = { rows, 1 };
        result_ptr->data.reserve(rows);
        for (size_t r = 0; r < rows; ++r) {
            double row_sum = 0.0;
            for (size_t c = 0; c < cols; ++c) {
                row_sum += to_double(arr->data[r * cols + c]);
            }
            result_ptr->data.push_back(row_sum);
        }
    }
    else {
        return nullptr; // Invalid axis
    }
    return result_ptr;
}

/**
  * @brief Helper to add two arrays together, element-wise, with broadcasting.
  * @param a The first array.
  * @param b The second array.
  * @return A shared_ptr to a new Array containing the sum. Returns nullptr on error.
  */
std::shared_ptr<Array> array_add(const std::shared_ptr<Array>& a, const std::shared_ptr<Array>& b) {
    if (!a || !b) return nullptr;

    // Case 1: Broadcasting a scalar-like array (e.g., [[5]]) to a matrix/vector
    if (b->data.size() == 1) {
        double scalar = to_double(b->data[0]);
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = a->shape;
        result_ptr->data.reserve(a->data.size());
        for (const auto& elem : a->data) {
            result_ptr->data.push_back(to_double(elem) + scalar);
        }
        return result_ptr;
    }
    // Case 2: Broadcasting a matrix/vector to a scalar-like array
    if (a->data.size() == 1) {
        double scalar = to_double(a->data[0]);
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = b->shape;
        result_ptr->data.reserve(b->data.size());
        for (const auto& elem : b->data) {
            result_ptr->data.push_back(scalar + to_double(elem));
        }
        return result_ptr;
    }

    // Case 3: Broadcasting a row vector to a matrix
    if (a->shape.size() == 2 && b->shape.size() == 2 && a->shape[0] > 1 && b->shape[0] == 1 && a->shape[1] == b->shape[1]) {
        auto result_ptr = std::make_shared<Array>(*a); // Copy matrix
        for (size_t r = 0; r < a->shape[0]; ++r) {
            for (size_t c = 0; c < a->shape[1]; ++c) {
                result_ptr->data[r * a->shape[1] + c] = to_double(a->data[r * a->shape[1] + c]) + to_double(b->data[c]);
            }
        }
        return result_ptr;
    }

    // Case 4: Standard element-wise addition for arrays of the same shape
    if (a->shape != b->shape) {
        // If no broadcasting rule applies and shapes are different, it's an error.
        return nullptr;
    }

    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = a->shape;
    result_ptr->data.reserve(a->data.size());
    for (size_t i = 0; i < a->data.size(); ++i) {
        result_ptr->data.push_back(to_double(a->data[i]) + to_double(b->data[i]));
    }
    return result_ptr;
}


/**
 * @brief Helper to subtract one array from another, element-wise.
 * @param a The array to subtract from (minuend).
 * @param b The array to subtract (subtrahend).
 * @return A shared_ptr to a new Array containing the results. Returns nullptr on error.
 */
std::shared_ptr<Array> array_subtract(const std::shared_ptr<Array>& a, const std::shared_ptr<Array>& b) {
    if (!a || !b || a->shape != b->shape) {
        return nullptr;
    }

    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = a->shape;
    result_ptr->data.reserve(a->data.size());

    for (size_t i = 0; i < a->data.size(); ++i) {
        result_ptr->data.push_back(to_double(a->data[i]) - to_double(b->data[i]));
    }
    return result_ptr;
}

// --- Helper Namespace for AI-specific utilities ---
namespace {
    /**
     * @brief Creates an Array with a given shape, initialized with random values.
     * Uses Glorot (Xavier) uniform initialization, a standard for neural networks.
     * @param shape The desired shape of the array.
     * @param fan_in The number of input units to the layer.
     * @param fan_out The number of output units from the layer.
     * @return A std::shared_ptr<Array> to the newly created and initialized array.
     */
    std::shared_ptr<Array> create_randomized_array(const std::vector<size_t>& shape, size_t fan_in, size_t fan_out) {
        auto array_ptr = std::make_shared<Array>();
        array_ptr->shape = shape;

        // Glorot/Xavier initialization helps prevent gradients from vanishing or exploding.
        double limit = std::sqrt(6.0 / (fan_in + fan_out));

        // Use a high-quality random number generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> distr(-limit, limit);

        size_t total_elements = std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
        array_ptr->data.reserve(total_elements);

        for (size_t i = 0; i < total_elements; ++i) {
            array_ptr->data.push_back(distr(gen));
        }

        return array_ptr;
    }

    /**
     * @brief Helper to multiply each element of an array by a scalar value.
     * @param scalar The double-precision number to multiply by.
     * @param arr A shared_ptr to the Array.
     * @return A shared_ptr to a new Array containing the results.
     */
    std::shared_ptr<Array> array_scalar_multiply(double scalar, const std::shared_ptr<Array>& arr) {
        if (!arr) return nullptr;
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = arr->shape;
        result_ptr->data.reserve(arr->data.size());

        for (const auto& elem : arr->data) {
            result_ptr->data.push_back(to_double(elem) * scalar);
        }
        return result_ptr;
    }

    // --- Helper for element-wise scalar division on an Array ---
    std::shared_ptr<Array> array_scalar_divide(const std::shared_ptr<Array>& arr, double divisor) {
        if (!arr || divisor == 0.0) return nullptr;
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = arr->shape;
        result_ptr->data.reserve(arr->data.size());
        for (const auto& elem : arr->data) {
            result_ptr->data.push_back(to_double(elem) / divisor);
        }
        return result_ptr;
    }

    /**
     * @brief Helper for element-wise multiplication of two arrays.
     */
    std::shared_ptr<Array> array_elementwise_multiply(const std::shared_ptr<Array>& a, const std::shared_ptr<Array>& b) {
        if (!a || !b || a->shape != b->shape) return nullptr;
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = a->shape;
        result_ptr->data.reserve(a->data.size());
        for (size_t i = 0; i < a->data.size(); ++i) {
            result_ptr->data.push_back(to_double(a->data[i]) * to_double(b->data[i]));
        }
        return result_ptr;
    }

    // --- Internal Implementations for Tensor Math ---
    // These are the core logic functions, now kept private to this file.
    // 
    // --- Helper for element-wise power on an Array ---
    std::shared_ptr<Array> array_power(const std::shared_ptr<Array>& base, double exponent) {
        if (!base) return nullptr;
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = base->shape;
        result_ptr->data.reserve(base->data.size());
        for (const auto& elem : base->data) {
            result_ptr->data.push_back(std::pow(to_double(elem), exponent));
        }
        return result_ptr;
    }

    /**
     * @brief Rotates a 4D kernel (filter) by 180 degrees.
     * This is needed for the d_input calculation, which is a full convolution.
     * @param kernel The kernel to rotate. Shape: [out_c, in_c, h, w]
     * @return A new shared_ptr<Array> containing the rotated kernel.
     */
    std::shared_ptr<Array> rotate180(const std::shared_ptr<Array>& kernel) {
        auto rotated_kernel = std::make_shared<Array>(*kernel); // Make a copy
        size_t h = kernel->shape[2];
        size_t w = kernel->shape[3];
        size_t plane_size = h * w;

        for (size_t i = 0; i < kernel->data.size() / plane_size; ++i) {
            size_t plane_start = i * plane_size;
            std::reverse(rotated_kernel->data.begin() + plane_start, rotated_kernel->data.begin() + plane_start + plane_size);
        }
        return rotated_kernel;
    }

    // SUM(array, [dimension]) -> number or array
    BasicValue internal_array_sum(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
        // 1. --- Argument Validation ---
        if (args.size() < 1 || args.size() > 2) {
            Error::set(8, vm.runtime_current_line); // Wrong number of arguments
            return 0.0;
        }
        if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
            Error::set(15, vm.runtime_current_line, "First argument to SUM must be an array.");
            return 0.0;
        }
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[0]);
        if (!arr_ptr || arr_ptr->data.empty()) {
            return 0.0; // Sum of empty array is 0
        }

        // 2. --- Backward Compatibility: Reduce to Scalar ---
        if (args.size() == 1) {
            double total = 0.0;
            for (const auto& val : arr_ptr->data) {
                total += to_double(val);
            }
            return total;
        }

        // 3. --- New Functionality: Reduce along a Dimension ---
        if (arr_ptr->shape.size() != 2) {
            Error::set(15, vm.runtime_current_line, "Dimensional reduction currently only supports 2D matrices.");
            return 0.0;
        }
        int dimension = static_cast<int>(to_double(args[1]));
        size_t rows = arr_ptr->shape[0];
        size_t cols = arr_ptr->shape[1];

        auto result_ptr = std::make_shared<Array>();

        if (dimension == 0) { // Reduce along rows -> result is a row vector of size 'cols'
            result_ptr->shape = { 1, cols };
            result_ptr->data.assign(cols, 0.0); // Initialize with zeros
            for (size_t r = 0; r < rows; ++r) {
                for (size_t c = 0; c < cols; ++c) {
                    result_ptr->data[c] = to_double(result_ptr->data[c]) + to_double(arr_ptr->data[r * cols + c]);
                }
            }
        }
        else if (dimension == 1) { // Reduce along columns -> result is a column vector of size 'rows'
            result_ptr->shape = { rows, 1 };
            result_ptr->data.reserve(rows);
            for (size_t r = 0; r < rows; ++r) {
                double row_total = 0.0;
                for (size_t c = 0; c < cols; ++c) {
                    row_total += to_double(arr_ptr->data[r * cols + c]);
                }
                result_ptr->data.push_back(row_total);
            }
        }
        else {
            Error::set(1, vm.runtime_current_line, "Invalid dimension for reduction. Must be 0 or 1.");
            return 0.0;
        }

        return result_ptr;
    }

    // New tensor-aware sum logic
    BasicValue internal_tensor_sum(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
        const auto& input_tensor = std::get<std::shared_ptr<Tensor>>(args[0]);
        if (!input_tensor || !input_tensor->data) {
            Error::set(3, vm.runtime_current_line, "Input to SUM is a null Tensor.");
            return {};
        }

        // --- Forward Pass ---
        double total = 0.0;
        for (const auto& val : input_tensor->data->data) {
            total += to_double(val);
        }

        // The result is a new Tensor containing the scalar sum.
        auto result_tensor = std::make_shared<Tensor>();
        auto result_array = std::make_shared<Array>();
        result_array->shape = { 1 };
        result_array->data = { total };
        result_tensor->data = result_array;

        result_tensor->parents = { input_tensor };

        // --- Backward Pass Definition ---
        result_tensor->backward_fn = [input_tensor](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
            // The derivative of sum() is 1. The gradient is broadcasted back to the shape of the input.
            // So, we create a new tensor with the same shape as the input, where every element
            // is the value of the incoming gradient.

            double grad_val = to_double(output_grad->data->data[0]);

            auto grad_array = std::make_shared<Array>();
            grad_array->shape = input_tensor->data->shape;
            grad_array->data.assign(input_tensor->data->data.size(), grad_val);

            auto final_grad_tensor = std::make_shared<Tensor>();
            final_grad_tensor->data = grad_array;

            std::vector<std::shared_ptr<Tensor>> grads;
            grads.push_back(final_grad_tensor);
            return grads;
            };

        return result_tensor;
    }

    // MATMUL(matrixA, matrixB) -> matrix
    // IS NOW IN TENSOR 
    // Performs standard linear algebra matrix multiplication.
    BasicValue internal_array_matmul(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
        if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
        if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
            Error::set(15, vm.runtime_current_line); return {};
        }

        const auto& a_ptr = std::get<std::shared_ptr<Array>>(args[0]);
        const auto& b_ptr = std::get<std::shared_ptr<Array>>(args[1]);

        if (!a_ptr || !b_ptr || a_ptr->shape.size() != 2 || b_ptr->shape.size() != 2) {
            Error::set(15, vm.runtime_current_line, "Both parameter must be matrices"); // Must be matrices
            return {};
        }

        size_t rows_a = a_ptr->shape[0];
        size_t cols_a = a_ptr->shape[1];
        size_t rows_b = b_ptr->shape[0];
        size_t cols_b = b_ptr->shape[1];

        if (cols_a != rows_b) {
            Error::set(15, vm.runtime_current_line, "Inner dimensions must match"); // Inner dimensions must match
            return {};
        }

        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = { rows_a, cols_b };
        result_ptr->data.resize(rows_a * cols_b);

        for (size_t r = 0; r < rows_a; ++r) {
            for (size_t c = 0; c < cols_b; ++c) {
                double dot_product = 0.0;
                for (size_t i = 0; i < cols_a; ++i) { // cols_a is the common dimension
                    dot_product += to_double(a_ptr->data[r * cols_a + i]) * to_double(b_ptr->data[i * cols_b + c]);
                }
                result_ptr->data[r * cols_b + c] = dot_product;
            }
        }
        return result_ptr;
    }

    /**
     * @brief Performs matrix multiplication on two Tensors, building the computational graph.
     * @param vm The interpreter instance.
     * @param args A vector containing two arguments: the Tensors to be multiplied.
     * @return A new Tensor containing the result and the gradient function for backpropagation.
     * @usage In BASIC: C_tensor = MATMUL(A_tensor, B_tensor)
     */
    BasicValue internal_tensor_matmul(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
        // 1. --- Argument Validation ---
        if (args.size() != 2) {
            Error::set(8, vm.runtime_current_line, "MATMUL requires exactly two arguments.");
            return {};
        }
        if (!std::holds_alternative<std::shared_ptr<Tensor>>(args[0]) || !std::holds_alternative<std::shared_ptr<Tensor>>(args[1])) {
            Error::set(15, vm.runtime_current_line, "Arguments to tensor MATMUL must be Tensors.");
            return {};
        }

        const auto& a_tensor_ptr = std::get<std::shared_ptr<Tensor>>(args[0]);
        const auto& b_tensor_ptr = std::get<std::shared_ptr<Tensor>>(args[1]);

        if (!a_tensor_ptr || !b_tensor_ptr) {
            Error::set(3, vm.runtime_current_line, "Input tensor(s) to MATMUL are null.");
            return {};
        }

        // 2. --- Forward Pass ---
        // Perform the actual matrix multiplication using your existing `builtin_matmul` function.
        // We call it with the `.data` members (which are Arrays) of our input Tensors.
        BasicValue forward_result_val = builtin_matmul(vm, { BasicValue{a_tensor_ptr->data}, BasicValue{b_tensor_ptr->data} });

        if (Error::get() != 0) {
            // Propagate any errors from the underlying matmul (e.g., shape mismatch).
            return {};
        }

        // 3. --- Result Tensor Creation ---
        auto result_tensor_ptr = std::make_shared<Tensor>();
        result_tensor_ptr->data = std::get<std::shared_ptr<Array>>(forward_result_val);

        // 4. --- Computational Graph Linkage ---
        // Record that this new tensor was created from 'a' and 'b'.
        result_tensor_ptr->parents = { a_tensor_ptr, b_tensor_ptr };

        // 5. --- Backward Function Definition ---
        // This lambda defines how to compute the gradients for the inputs (A and B)
        // given the gradient of the output (C). This is the core of the chain rule for this operation.
        result_tensor_ptr->backward_fn = [a_tensor_ptr, b_tensor_ptr, &vm](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
            // The gradient of the loss with respect to input A is: grad_C @ B.T
            // The gradient of the loss with respect to input B is: A.T @ grad_C
            // where grad_C is the gradient flowing back from the subsequent node (output_grad).

            // Calculate gradient for input A
            auto b_transposed_val = builtin_transpose(vm, { BasicValue{b_tensor_ptr->data} });
            if (Error::get() != 0) return {};
            auto grad_a_val = builtin_matmul(vm, { BasicValue{output_grad->data}, b_transposed_val });
            if (Error::get() != 0) return {};

            // Calculate gradient for input B
            auto a_transposed_val = builtin_transpose(vm, { BasicValue{a_tensor_ptr->data} });
            if (Error::get() != 0) return {};
            auto grad_b_val = builtin_matmul(vm, { a_transposed_val, BasicValue{output_grad->data} });
            if (Error::get() != 0) return {};

            // Wrap the resulting gradient Arrays in new Tensors.
            auto grad_a_tensor = std::make_shared<Tensor>();
            grad_a_tensor->data = std::get<std::shared_ptr<Array>>(grad_a_val);

            auto grad_b_tensor = std::make_shared<Tensor>();
            grad_b_tensor->data = std::get<std::shared_ptr<Array>>(grad_b_val);

            // Return the computed gradients for each parent. The order must match the `parents` vector.
            return { grad_a_tensor, grad_b_tensor };
            };

        // 6. --- Return Value ---
        // Return the new result Tensor, wrapped in a BasicValue.
        return result_tensor_ptr;
    }


    /**
     * @brief Applies the sigmoid activation function to a Tensor, building the graph.
     */
    BasicValue internal_tensor_sigmoid(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
        if (args.size() != 1 || !std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
            Error::set(15, vm.runtime_current_line, "SIGMOID requires a single Tensor argument.");
            return {};
        }
        const auto& input_tensor = std::get<std::shared_ptr<Tensor>>(args[0]);
        if (!input_tensor || !input_tensor->data) return {};

        // Forward pass: calculate sigmoid
        auto result_array = std::make_shared<Array>();
        result_array->shape = input_tensor->data->shape;
        result_array->data.reserve(input_tensor->data->data.size());
        for (const auto& val : input_tensor->data->data) {
            result_array->data.push_back(1.0 / (1.0 + std::exp(-to_double(val))));
        }

        auto result_tensor = std::make_shared<Tensor>();
        result_tensor->data = result_array;
        result_tensor->parents = { input_tensor };

        // Backward pass: derivative is sigmoid(x) * (1 - sigmoid(x))
        result_tensor->backward_fn = [result_tensor](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
            auto ones_array = std::make_shared<Array>();
            ones_array->shape = result_tensor->data->shape;
            ones_array->data.assign(result_tensor->data->data.size(), 1.0);

            auto one_minus_sigmoid = array_subtract(ones_array, result_tensor->data);
            auto derivative = array_elementwise_multiply(result_tensor->data, one_minus_sigmoid);
            auto final_grad_data = array_elementwise_multiply(output_grad->data, derivative);

            // --- FIX START ---
            // Explicitly create the tensor before returning.
            auto final_grad_tensor = std::make_shared<Tensor>();
            final_grad_tensor->data = final_grad_data;
            std::vector<std::shared_ptr<Tensor>> grads = { final_grad_tensor };
            return grads;
            // --- FIX END ---
            };

        return result_tensor;
    }
} // end anonymous namespace

// --- FORWARD DECLARATIONS of new Tensor operation functions ---
BasicValue tensor_add(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b);
BasicValue tensor_subtract(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b);
BasicValue tensor_elementwise_multiply(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b);
BasicValue tensor_power(NeReLaBasic& vm, const BasicValue& base, const BasicValue& exponent);


// --- BUILT-IN FUNCTION IMPLEMENTATIONS ---
// --- Tensor things ---
// --- AI & Neural Network Functions ---

BasicValue tensor_scalar_divide(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b) {
    const auto& tensor_a = std::get<std::shared_ptr<Tensor>>(a);
    const double scalar_b = to_double(b);

    if (scalar_b == 0.0) {
        Error::set(2, vm.runtime_current_line, "Division by zero.");
        return {};
    }

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = array_scalar_divide(tensor_a->data, scalar_b);
    result_tensor->parents = { tensor_a };

    result_tensor->backward_fn = [tensor_a, scalar_b](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        auto grad_data = array_scalar_divide(output_grad->data, scalar_b);
        auto final_grad = std::make_shared<Tensor>();
        final_grad->data = grad_data;
        std::vector<std::shared_ptr<Tensor>> grads;
        grads.push_back(final_grad);
        return grads;
        };
    return result_tensor;
}

/**
 * @brief Gets the underlying Data Array from a Tensor.
 * @param vm The interpreter instance.
 * @param args A vector containing one argument: the Tensor.
 * @return The data as a BasicValue holding an Array.
 */
BasicValue builtin_toarray(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line, "TOARRAY requires exactly one Tensor argument.");
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
        Error::set(15, vm.runtime_current_line, "Argument to TOARRAY must be a Tensor.");
        return {};
    }

    const auto& tensor_ptr = std::get<std::shared_ptr<Tensor>>(args[0]);
    if (!tensor_ptr) {
        Error::set(3, vm.runtime_current_line, "Cannot get data from a null Tensor.");
        return {};
    }

    return tensor_ptr->data;
}

BasicValue tensor_power(NeReLaBasic& vm, const BasicValue& base, const BasicValue& exponent) {
    const auto& base_tensor = std::get<std::shared_ptr<Tensor>>(base);
    const double exp_val = to_double(exponent);

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = array_power(base_tensor->data, exp_val);
    if (!result_tensor->data) {
        Error::set(15, vm.runtime_current_line, "Power operation failed.");
        return {};
    }

    result_tensor->parents = { base_tensor };
    result_tensor->backward_fn = [base_tensor, exp_val](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        // Derivative of u^n is n * u^(n-1) * du
        auto one_less_exp = array_power(base_tensor->data, exp_val - 1.0);
        auto n_times_u_n_minus_1 = array_scalar_multiply(exp_val, one_less_exp);
        auto final_grad_data = array_elementwise_multiply(output_grad->data, n_times_u_n_minus_1);

        auto final_grad_tensor = std::make_shared<Tensor>();
        final_grad_tensor->data = final_grad_data;

        std::vector<std::shared_ptr<Tensor>> grads = { final_grad_tensor };
        return grads;
        };

    return result_tensor;
}
// --- AI Factory Functions ---
/**
 * @brief Converts an Array into a Tensor, making it ready for autodiff.
 * @param vm The interpreter instance.
 * @param args A vector containing one argument: the Array to convert.
 * @return A new Tensor containing the data from the input Array.
 */
BasicValue builtin_to_tensor(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line, "TOTENSOR requires exactly one Array argument.");
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        Error::set(15, vm.runtime_current_line, "Argument to TOTENSOR must be an Array.");
        return {};
    }

    const auto& array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    if (!array_ptr) {
        Error::set(3, vm.runtime_current_line, "Cannot convert a null Array to a Tensor.");
        return {};
    }

    auto tensor_ptr = std::make_shared<Tensor>();
    tensor_ptr->data = array_ptr;
    // The new tensor has no parents and no backward function; it's a leaf node in the graph.
    return tensor_ptr;
}

/**
 * @brief Performs a 2D convolution operation, including the backward pass for autodiff.
 * @param vm The interpreter instance.
 * @param args A vector: input_tensor, kernel_tensor, bias_tensor, stride, padding
 * @return A new Tensor containing the result (feature map).
 */
BasicValue builtin_conv2d(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 5) { Error::set(8, vm.runtime_current_line, "CONV2D requires 5 arguments."); return {}; }
    const auto& input_tensor = std::get<std::shared_ptr<Tensor>>(args[0]);
    const auto& kernel_tensor = std::get<std::shared_ptr<Tensor>>(args[1]);
    const auto& bias_tensor = std::get<std::shared_ptr<Tensor>>(args[2]);
    int stride = static_cast<int>(to_double(args[3]));
    int padding = static_cast<int>(to_double(args[4]));

    // --- FORWARD PASS ---
    // (The forward pass logic remains the same as before)
    size_t in_channels = input_tensor->data->shape[1];
    size_t in_height = input_tensor->data->shape[2];
    size_t in_width = input_tensor->data->shape[3];
    size_t out_channels = kernel_tensor->data->shape[0];
    size_t kernel_h = kernel_tensor->data->shape[2];
    size_t kernel_w = kernel_tensor->data->shape[3];
    size_t out_height = static_cast<size_t>(std::floor((in_height + 2 * padding - kernel_h) / stride)) + 1;
    size_t out_width = static_cast<size_t>(std::floor((in_width + 2 * padding - kernel_w) / stride)) + 1;

    auto result_array = std::make_shared<Array>();
    result_array->shape = { 1, out_channels, out_height, out_width };
    result_array->data.assign(out_channels * out_height * out_width, 0.0);

    for (size_t oc = 0; oc < out_channels; ++oc) {
        for (size_t y = 0; y < out_height; ++y) {
            for (size_t x = 0; x < out_width; ++x) {
                double sum = 0.0;
                for (size_t ic = 0; ic < in_channels; ++ic) {
                    for (size_t ky = 0; ky < kernel_h; ++ky) {
                        for (size_t kx = 0; kx < kernel_w; ++kx) {
                            int input_y = y * stride + ky - padding;
                            int input_x = x * stride + kx - padding;
                            if (input_y >= 0 && input_y < in_height && input_x >= 0 && input_x < in_width) {
                                double input_val = to_double(input_tensor->data->data[ic * (in_height * in_width) + input_y * in_width + input_x]);
                                double kernel_val = to_double(kernel_tensor->data->data[oc * (in_channels * kernel_h * kernel_w) + ic * (kernel_h * kernel_w) + ky * kernel_w + kx]);
                                sum += input_val * kernel_val;
                            }
                        }
                    }
                }
                sum += to_double(bias_tensor->data->data[oc]);
                result_array->data[oc * (out_height * out_width) + y * out_width + x] = sum;
            }
        }
    }

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = result_array;
    result_tensor->parents = { input_tensor, kernel_tensor, bias_tensor };

    // --- BACKWARD PASS DEFINITION ---
    result_tensor->backward_fn = [=](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        auto d_input_arr = std::make_shared<Array>();
        d_input_arr->shape = input_tensor->data->shape;
        d_input_arr->data.assign(input_tensor->data->size(), 0.0);

        auto d_kernel_arr = std::make_shared<Array>();
        d_kernel_arr->shape = kernel_tensor->data->shape;
        d_kernel_arr->data.assign(kernel_tensor->data->size(), 0.0);

        auto d_bias_arr = std::make_shared<Array>();
        d_bias_arr->shape = bias_tensor->data->shape;
        d_bias_arr->data.assign(bias_tensor->data->size(), 0.0);

        auto rotated_kernel = rotate180(kernel_tensor->data);

        // This loop calculates all three gradients simultaneously
        for (size_t oc = 0; oc < out_channels; ++oc) {
            for (size_t y = 0; y < out_height; ++y) {
                for (size_t x = 0; x < out_width; ++x) {
                    double grad_out_val = to_double(output_grad->data->data[oc * (out_height * out_width) + y * out_width + x]);

                    // --- Calculate Gradient for Bias (d_bias) ---
                    // The gradient for a bias is simply the sum of the gradients from the feature map it influenced.
                    d_bias_arr->data[oc] = to_double(d_bias_arr->data[oc]) + grad_out_val;

                    // --- Calculate Gradients for Input (d_input) and Kernel (d_kernel) ---
                    for (size_t ic = 0; ic < in_channels; ++ic) {
                        for (size_t ky = 0; ky < kernel_h; ++ky) {
                            for (size_t kx = 0; kx < kernel_w; ++kx) {
                                int input_y = y * stride + ky - padding;
                                int input_x = x * stride + kx - padding;

                                if (input_y >= 0 && input_y < in_height && input_x >= 0 && input_x < in_width) {
                                    // Calculate d_kernel: convolve input with output_grad
                                    double input_val = to_double(input_tensor->data->data[ic * (in_height * in_width) + input_y * in_width + input_x]);
                                    size_t d_kernel_idx = oc * (in_channels * kernel_h * kernel_w) + ic * (kernel_h * kernel_w) + ky * kernel_w + kx;
                                    d_kernel_arr->data[d_kernel_idx] = to_double(d_kernel_arr->data[d_kernel_idx]) + (input_val * grad_out_val);

                                    // Calculate d_input: convolve output_grad with rotated kernel
                                    double rotated_kernel_val = to_double(rotated_kernel->data[d_kernel_idx]);
                                    size_t d_input_idx = ic * (in_height * in_width) + input_y * in_width + input_x;
                                    d_input_arr->data[d_input_idx] = to_double(d_input_arr->data[d_input_idx]) + (rotated_kernel_val * grad_out_val);
                                }
                            }
                        }
                    }
                }
            }
        }

        auto d_input = std::make_shared<Tensor>(); d_input->data = d_input_arr;
        auto d_kernel = std::make_shared<Tensor>(); d_kernel->data = d_kernel_arr;
        auto d_bias = std::make_shared<Tensor>(); d_bias->data = d_bias_arr;

        return { d_input, d_kernel, d_bias };
        };

    return result_tensor;
}


/**
 * @brief Performs a 2D max pooling operation.
 * @param vm The interpreter instance.
 * @param args A vector: input_tensor, pool_size, stride
 * @return A new Tensor containing the downsampled result.
 */
BasicValue builtin_maxpool2d(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 3) { Error::set(8, vm.runtime_current_line, "MAXPOOL2D requires 3 arguments."); return {}; }
    const auto& input = std::get<std::shared_ptr<Tensor>>(args[0]);
    int pool_size = static_cast<int>(to_double(args[1]));
    int stride = static_cast<int>(to_double(args[2]));

    size_t channels = input->data->shape[1];
    size_t in_height = input->data->shape[2];
    size_t in_width = input->data->shape[3];

    size_t out_height = static_cast<size_t>(std::floor((in_height - pool_size) / stride)) + 1;
    size_t out_width = static_cast<size_t>(std::floor((in_width - pool_size) / stride)) + 1;

    auto result_array = std::make_shared<Array>();
    result_array->shape = { 1, channels, out_height, out_width };
    result_array->data.resize(channels * out_height * out_width);

    // This array will store the indices of the max values, needed for backpropagation.
    auto indices_array = std::make_shared<Array>();
    indices_array->shape = result_array->shape;
    indices_array->data.resize(result_array->size());

    for (size_t c = 0; c < channels; ++c) {
        for (size_t y = 0; y < out_height; ++y) {
            for (size_t x = 0; x < out_width; ++x) {
                double max_val = -std::numeric_limits<double>::infinity();
                size_t max_index = 0;
                for (int py = 0; py < pool_size; ++py) {
                    for (int px = 0; px < pool_size; ++px) {
                        int input_y = y * stride + py;
                        int input_x = x * stride + px;
                        size_t current_index = c * (in_height * in_width) + input_y * in_width + input_x;
                        double current_val = to_double(input->data->data[current_index]);
                        if (current_val > max_val) {
                            max_val = current_val;
                            max_index = current_index;
                        }
                    }
                }
                result_array->data[c * (out_height * out_width) + y * out_width + x] = max_val;
                indices_array->data[c * (out_height * out_width) + y * out_width + x] = static_cast<double>(max_index);
            }
        }
    }

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = result_array;
    result_tensor->parents = { input };

    // Backward pass for max pooling routes the gradient to the location of the max value.
    result_tensor->backward_fn = [input_shape = input->data->shape, indices_array](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        auto input_grad_array = std::make_shared<Array>();
        input_grad_array->shape = input_shape;
        input_grad_array->data.assign(input_shape[0] * input_shape[1] * input_shape[2] * input_shape[3], 0.0);

        for (size_t i = 0; i < output_grad->data->data.size(); ++i) {
            size_t max_idx = static_cast<size_t>(to_double(indices_array->data[i]));
            // The += operator is not defined for std::variant (BasicValue).
            // We must perform the operation manually.
            double current_grad = to_double(input_grad_array->data[max_idx]);
            double new_grad = to_double(output_grad->data->data[i]);
            input_grad_array->data[max_idx] = current_grad + new_grad;
        }

        auto final_grad = std::make_shared<Tensor>();
        final_grad->data = input_grad_array;
        std::vector<std::shared_ptr<Tensor>> grads;
        grads.push_back(final_grad);
        return grads;
        };

    return result_tensor;
}


/**
 * @brief Creates and initializes a neural network layer.
 * @param vm The interpreter instance.
 * @param args Args from BASIC: layer_type_string$, options_map
 * @return A Map representing the configured layer.
 * @usage In BASIC: hidden_layer = CREATE_LAYER("DENSE", {input_size: 2, units: 3})
 */
BasicValue builtin_create_layer(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line, "CREATE_LAYER requires two arguments: type_string, options_map.");
        return {};
    }
    if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::shared_ptr<Map>>(args[1])) {
        Error::set(15, vm.runtime_current_line, "Invalid argument types for CREATE_LAYER.");
        return {};
    }

    std::string layer_type = to_upper(std::get<std::string>(args[0]));
    const auto& options_map_ptr = std::get<std::shared_ptr<Map>>(args[1]);
    if (!options_map_ptr) {
        Error::set(3, vm.runtime_current_line, "Options map is null.");
        return {};
    }

    auto layer_result_ptr = std::make_shared<Map>();
    layer_result_ptr->data["type"] = layer_type;

    if (layer_type == "DENSE") {
        // --- Dense Layer Creation ---
        if (!options_map_ptr->data.count("input_size") || !options_map_ptr->data.count("units")) {
            Error::set(1, vm.runtime_current_line, "DENSE layer options must include 'input_size' and 'units'.");
            return {};
        }

        size_t input_size = static_cast<size_t>(to_double(options_map_ptr->data["input_size"]));
        size_t units = static_cast<size_t>(to_double(options_map_ptr->data["units"]));

        // Create and initialize weights tensor
        auto weights_tensor = std::make_shared<Tensor>();
        weights_tensor->data = create_randomized_array({ input_size, units }, input_size, units);
        layer_result_ptr->data["weights"] = weights_tensor;

        // Create and initialize bias tensor (initialized to zeros)
        auto bias_tensor = std::make_shared<Tensor>();
        auto bias_array = std::make_shared<Array>();
        bias_array->shape = { 1, units }; // Bias is a row vector
        bias_array->data.assign(units, 0.0);
        bias_tensor->data = bias_array;
        layer_result_ptr->data["bias"] = bias_tensor;

    }
    else if (layer_type == "CONV2D") {
        size_t in_channels = static_cast<size_t>(to_double(options_map_ptr->data.at("in_channels")));
        size_t out_channels = static_cast<size_t>(to_double(options_map_ptr->data.at("out_channels")));
        size_t kernel_size = static_cast<size_t>(to_double(options_map_ptr->data.at("kernel_size")));

        // Kernel shape: [out_channels, in_channels, kernel_size, kernel_size]
        auto weights_tensor = std::make_shared<Tensor>();
        weights_tensor->data = create_randomized_array({ out_channels, in_channels, kernel_size, kernel_size }, in_channels * kernel_size * kernel_size, out_channels * kernel_size * kernel_size);
        layer_result_ptr->data["weights"] = weights_tensor;

        auto bias_tensor = std::make_shared<Tensor>();
        auto bias_array = std::make_shared<Array>();
        bias_array->shape = { out_channels }; // Bias is one per output channel
        bias_array->data.assign(out_channels, 0.0);
        bias_tensor->data = bias_array;
        layer_result_ptr->data["bias"] = bias_tensor;

    }
    else if (layer_type == "MAXPOOL2D") {
        // No weights or biases needed for max pooling. The options are stored above.
    }
    else {
        Error::set(1, vm.runtime_current_line, "Unknown layer type: " + layer_type);
        return {};
    }

    return layer_result_ptr;
}

/**
 * @brief Creates an optimizer configuration map.
 * @param vm The interpreter instance.
 * @param args Args from BASIC: optimizer_type_string$, options_map
 * @return A Map representing the configured optimizer.
 * @usage In BASIC: sgd = CREATE_OPTIMIZER("SGD", {learning_rate: 0.01})
 */
BasicValue builtin_create_optimizer(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line, "CREATE_OPTIMIZER requires two arguments: type_string, options_map.");
        return {};
    }
    if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::shared_ptr<Map>>(args[1])) {
        Error::set(15, vm.runtime_current_line, "Invalid argument types for CREATE_OPTIMIZER.");
        return {};
    }

    std::string optimizer_type = to_upper(std::get<std::string>(args[0]));
    const auto& options_map_ptr = std::get<std::shared_ptr<Map>>(args[1]);
    if (!options_map_ptr) {
        Error::set(3, vm.runtime_current_line, "Options map is null.");
        return {};
    }

    auto optimizer_result_ptr = std::make_shared<Map>();
    optimizer_result_ptr->data["type"] = optimizer_type;

    if (optimizer_type == "SGD") {
        if (!options_map_ptr->data.count("learning_rate")) {
            Error::set(1, vm.runtime_current_line, "SGD optimizer requires 'learning_rate' in options.");
            return {};
        }
        optimizer_result_ptr->data["lr"] = options_map_ptr->data.at("learning_rate");
    }
    else {
        Error::set(1, vm.runtime_current_line, "Unknown optimizer type: " + optimizer_type);
        return {};
    }

    return optimizer_result_ptr;
}

/**
 * @brief Adds two Tensors, handling broadcasting and its derivative correctly.
 */
BasicValue tensor_add(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b) {
    const auto& a_tensor = std::get<std::shared_ptr<Tensor>>(a);
    const auto& b_tensor = std::get<std::shared_ptr<Tensor>>(b);

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = array_add(a_tensor->data, b_tensor->data);
    if (!result_tensor->data) {
        Error::set(15, vm.runtime_current_line, "Tensor shapes are not compatible for addition.");
        return {};
    }

    result_tensor->parents = { a_tensor, b_tensor };
    result_tensor->backward_fn = [a_tensor, b_tensor](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        auto grad_a = output_grad;
        auto grad_b = output_grad;

        // Handle gradient for operand A if it was broadcast
        if (a_tensor->data->data.size() < output_grad->data->data.size()) {
            auto summed_grad_data = array_sum_along_axis(output_grad->data, 0);
            grad_a = std::make_shared<Tensor>();
            grad_a->data = summed_grad_data;
        }

        // Handle gradient for operand B if it was broadcast (the common case for a bias term)
        if (b_tensor->data->data.size() < output_grad->data->data.size()) {
            auto summed_grad_data = array_sum_along_axis(output_grad->data, 0);
            grad_b = std::make_shared<Tensor>();
            grad_b->data = summed_grad_data;
        }

        std::vector<std::shared_ptr<Tensor>> grads;
        grads.push_back(grad_a);
        grads.push_back(grad_b);
        return grads;
        };

    return result_tensor;
}


/**
 * @brief Subtracts two Tensors, building the graph.
 */
BasicValue tensor_subtract(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b) {
    const auto& a_tensor = std::get<std::shared_ptr<Tensor>>(a);
    const auto& b_tensor = std::get<std::shared_ptr<Tensor>>(b);

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = array_subtract(a_tensor->data, b_tensor->data);
    if (!result_tensor->data) {
        Error::set(15, vm.runtime_current_line, "Tensor shapes are not compatible for subtraction.");
        return {};
    }

    result_tensor->parents = { a_tensor, b_tensor };
    result_tensor->backward_fn = [a_tensor, b_tensor](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        // grad(A) is output_grad. grad(B) is -output_grad.
        auto neg_grad_data = array_scalar_multiply(-1.0, output_grad->data);

        // --- FIX START ---
        // Explicitly create the tensor before returning.
        auto neg_grad_tensor = std::make_shared<Tensor>();
        neg_grad_tensor->data = neg_grad_data;
        std::vector<std::shared_ptr<Tensor>> grads = { output_grad, neg_grad_tensor };
        return grads;
        // --- FIX END ---
        };
    return result_tensor;
}

/**
 * @brief Element-wise multiplication of two Tensors, building the graph.
 */
BasicValue tensor_elementwise_multiply(NeReLaBasic& vm, const BasicValue& a, const BasicValue& b) {
    const auto& a_tensor = std::get<std::shared_ptr<Tensor>>(a);
    const auto& b_tensor = std::get<std::shared_ptr<Tensor>>(b);

    auto result_tensor = std::make_shared<Tensor>();
    result_tensor->data = array_elementwise_multiply(a_tensor->data, b_tensor->data);
    if (!result_tensor->data) {
        Error::set(15, vm.runtime_current_line, "Tensor shapes not compatible for element-wise multiplication.");
        return {};
    }

    result_tensor->parents = { a_tensor, b_tensor };
    result_tensor->backward_fn = [a_tensor, b_tensor](std::shared_ptr<Tensor> output_grad) -> std::vector<std::shared_ptr<Tensor>> {
        // grad(A) = output_grad * B
        // grad(B) = output_grad * A
        auto grad_a_data = array_elementwise_multiply(output_grad->data, b_tensor->data);
        auto grad_b_data = array_elementwise_multiply(output_grad->data, a_tensor->data);

        // --- FIX START ---
        // Explicitly create tensor objects before returning.
        auto grad_a_tensor = std::make_shared<Tensor>();
        grad_a_tensor->data = grad_a_data;
        auto grad_b_tensor = std::make_shared<Tensor>();
        grad_b_tensor->data = grad_b_data;

        std::vector<std::shared_ptr<Tensor>> grads = { grad_a_tensor, grad_b_tensor };
        return grads;
        // --- FIX END ---
        };
    return result_tensor;
}

BasicValue builtin_matmul(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }

    // Dispatch based on the type of the first argument.
    if (std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
        return internal_tensor_matmul(vm, args); // Note: This now calls the full tensor version
    }
    if (std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        return internal_array_matmul(vm, args); // Your original implementation
    }

    Error::set(15, vm.runtime_current_line, "MATMUL requires Array or Tensor arguments.");
    return {};
}


BasicValue builtin_sigmoid(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) { Error::set(8, vm.runtime_current_line); return {}; }

    if (std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
        return internal_tensor_sigmoid(vm, args); // This was already complete in the last step
    }
    if (std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        const auto& input_array = std::get<std::shared_ptr<Array>>(args[0]);
        auto result_array = std::make_shared<Array>();
        result_array->shape = input_array->shape;
        result_array->data.reserve(input_array->data.size());
        for (const auto& val : input_array->data) {
            result_array->data.push_back(1.0 / (1.0 + std::exp(-to_double(val))));
        }
        return result_array;
    }

    Error::set(15, vm.runtime_current_line, "SIGMOID requires an Array or Tensor argument.");
    return {};
}


/**
 * @brief Performs backpropagation on the computational graph.
 * @param vm The interpreter instance.
 * @param args A vector containing one argument: the final loss Tensor.
 * @usage In BASIC: BACKWARD loss_tensor
 */
BasicValue builtin_backward(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line, "BACKWARD requires exactly one argument (the loss tensor).");
        return false;
    }
    if (!std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
        Error::set(15, vm.runtime_current_line, "Argument to BACKWARD must be a Tensor.");
        return false;
    }

    auto loss_tensor = std::get<std::shared_ptr<Tensor>>(args[0]);

    std::vector<std::shared_ptr<Tensor>> sorted_graph;
    std::unordered_set<Tensor*> visited;

    std::function<void(std::shared_ptr<Tensor>)> visit =
        [&](std::shared_ptr<Tensor> node) {
        if (!node || visited.count(node.get())) return;
        visited.insert(node.get());
        for (auto& parent : node->parents) {
            visit(parent);
        }
        sorted_graph.push_back(node);
        };

    visit(loss_tensor);

    auto grad_one_data = std::make_shared<Array>();
    grad_one_data->data = { 1.0 };
    grad_one_data->shape = { 1 };
    auto grad_tensor = std::make_shared<Tensor>();
    grad_tensor->data = grad_one_data;
    loss_tensor->grad = grad_tensor;


    // Iterate through the sorted graph in reverse order to backpropagate.
    for (auto it = sorted_graph.rbegin(); it != sorted_graph.rend(); ++it) {
        auto& node = *it;
        if (node->backward_fn) {
            auto parent_grads = node->backward_fn(node->grad);

            for (size_t i = 0; i < node->parents.size(); ++i) {
                auto& parent = node->parents[i];
                if (!parent_grads[i] || !parent_grads[i]->data) continue;

                if (parent->grad && parent->grad->data) {
                    // Accumulate gradient: parent.grad += new_grad
                    parent->grad->data = array_add(parent->grad->data, parent_grads[i]->data);
                }
                else {
                    // First time seeing a gradient for this tensor
                    parent->grad = parent_grads[i];
                }
            }
        }
    }

    return false; // Procedures return a dummy value.
}


/**
 * @brief Updates a model's parameters using an optimizer configuration and clears gradients.
 * @param vm The interpreter instance.
 * @param args A vector containing two arguments: the model and the optimizer.
 * @return The updated model.
 * @usage In BASIC: new_model = UPDATE(model, optimizer)
 */
BasicValue builtin_update(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line, "UPDATE requires two arguments: model, optimizer.");
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        Error::set(15, vm.runtime_current_line, "First argument to UPDATE must be a model (an array of layers).");
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Map>>(args[1])) {
        Error::set(15, vm.runtime_current_line, "Second argument to UPDATE must be an optimizer map.");
        return {};
    }

    const auto& model_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const auto& optimizer_map_ptr = std::get<std::shared_ptr<Map>>(args[1]);

    if (!model_array_ptr || !optimizer_map_ptr) {
        Error::set(3, vm.runtime_current_line, "Model or optimizer is null.");
        return {};
    }

    double lr = to_double(optimizer_map_ptr->data["lr"]);

    for (const auto& layer_val : model_array_ptr->data) {
        const auto& layer_map = std::get<std::shared_ptr<Map>>(layer_val);
        auto& weights = std::get<std::shared_ptr<Tensor>>(layer_map->data["weights"]);
        auto& bias = std::get<std::shared_ptr<Tensor>>(layer_map->data["bias"]);

        // Apply the update rule: param = param - lr * param.grad
        if (weights && weights->grad && weights->grad->data) {
            auto delta_w = array_scalar_multiply(lr, weights->grad->data);
            weights->data = array_subtract(weights->data, delta_w);
        }

        if (bias && bias->grad && bias->grad->data) {
            auto delta_b = array_scalar_multiply(lr, bias->grad->data);
            bias->data = array_subtract(bias->data, delta_b);
        }

        // --- THIS IS THE FIX ---
        // Clear the gradients after they have been used for the update.
        // This is equivalent to optimizer.zero_grad() in other frameworks.
        if (weights) {
            weights->grad = nullptr;
        }
        if (bias) {
            bias->grad = nullptr;
        }
    }

    return args[0];
}

/**
 * @brief Saves a model's structure and weights to a JSON file.
 * @param vm The interpreter instance.
 * @param args Args from BASIC: model_array, filename_string
 * @return A dummy boolean value (as this is a procedure).
 */
BasicValue builtin_save_model(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line, "SAVE_MODEL requires two arguments: model_array, filename.");
        return false;
    }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::string>(args[1])) {
        Error::set(15, vm.runtime_current_line, "Invalid argument types for SAVE_MODEL.");
        return false;
    }

    const auto& model_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const std::string filename = std::get<std::string>(args[1]);

    if (!model_array_ptr) { Error::set(3, vm.runtime_current_line, "Model is null."); return false; }

    nlohmann::json j_model = nlohmann::json::array();

    for (const auto& layer_val : model_array_ptr->data) {
        if (!std::holds_alternative<std::shared_ptr<Map>>(layer_val)) continue;
        const auto& layer_map = std::get<std::shared_ptr<Map>>(layer_val);

        nlohmann::json j_layer;
        j_layer["type"] = to_string(layer_map->data["type"]);

        // Save weights
        const auto& weights_tensor = std::get<std::shared_ptr<Tensor>>(layer_map->data["weights"]);
        j_layer["weights"]["shape"] = weights_tensor->data->shape;
        // --- FIX: Manually convert each BasicValue to a JSON value ---
        nlohmann::json j_weights_data = nlohmann::json::array();
        for (const auto& val : weights_tensor->data->data) {
            j_weights_data.push_back(basic_to_json_value(val));
        }
        j_layer["weights"]["data"] = j_weights_data;

        // Save bias
        const auto& bias_tensor = std::get<std::shared_ptr<Tensor>>(layer_map->data["bias"]);
        j_layer["bias"]["shape"] = bias_tensor->data->shape;
        // --- FIX: Manually convert each BasicValue to a JSON value ---
        nlohmann::json j_bias_data = nlohmann::json::array();
        for (const auto& val : bias_tensor->data->data) {
            j_bias_data.push_back(basic_to_json_value(val));
        }
        j_layer["bias"]["data"] = j_bias_data;

        j_model.push_back(j_layer);
    }

    std::ofstream outfile(filename);
    if (!outfile) {
        Error::set(12, vm.runtime_current_line, "Failed to open file for writing: " + filename);
        return false;
    }

    outfile << j_model.dump(4); // pretty-print with 4-space indent
    return true; // Success
}

/**
 * @brief Loads a model from a JSON file.
 * @param vm The interpreter instance.
 * @param args Args from BASIC: filename_string
 * @return A new model (Array of Maps) populated from the file.
 */
BasicValue builtin_load_model(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line, "LOAD_MODEL requires a single filename argument.");
        return {};
    }
    if (!std::holds_alternative<std::string>(args[0])) {
        Error::set(15, vm.runtime_current_line, "Argument to LOAD_MODEL must be a string.");
        return {};
    }

    const std::string filename = std::get<std::string>(args[0]);
    std::ifstream infile(filename);
    if (!infile) {
        Error::set(6, vm.runtime_current_line, "Model file not found: " + filename);
        return {};
    }

    nlohmann::json j_model;
    try {
        infile >> j_model;
    }
    catch (const nlohmann::json::parse_error& e) {
        Error::set(1, vm.runtime_current_line, "Invalid JSON format in model file. " + std::string(e.what()));
        return {};
    }

    auto new_model_ptr = std::make_shared<Array>();
    new_model_ptr->shape = { j_model.size() };

    for (const auto& j_layer : j_model) {
        auto layer_map = std::make_shared<Map>();
        layer_map->data["type"] = j_layer["type"].get<std::string>();

        // Load Weights
        auto weights_tensor = std::make_shared<Tensor>();
        auto weights_array = std::make_shared<Array>();
        weights_array->shape = j_layer["weights"]["shape"].get<std::vector<size_t>>();
        // --- FIX: Manually convert each JSON value back to a BasicValue ---
        for (const auto& j_val : j_layer["weights"]["data"]) {
            weights_array->data.push_back(json_to_basic_value(j_val));
        }
        weights_tensor->data = weights_array;
        layer_map->data["weights"] = weights_tensor;

        // Load Bias
        auto bias_tensor = std::make_shared<Tensor>();
        auto bias_array = std::make_shared<Array>();
        bias_array->shape = j_layer["bias"]["shape"].get<std::vector<size_t>>();
        // --- FIX: Manually convert each JSON value back to a BasicValue ---
        for (const auto& j_val : j_layer["bias"]["data"]) {
            bias_array->data.push_back(json_to_basic_value(j_val));
        }
        bias_tensor->data = bias_array;
        layer_map->data["bias"] = bias_tensor;

        new_model_ptr->data.push_back(layer_map);
    }

    return new_model_ptr;
}

// --- DISPATCHER FUNCTION for SUM ---
// This will be the public-facing function registered with the interpreter.
BasicValue builtin_sum(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.empty() || args.size() > 2) {
        Error::set(8, vm.runtime_current_line, "SUM requires 1 or 2 arguments.");
        return {};
    }

    // --- Type Dispatching ---
    if (std::holds_alternative<std::shared_ptr<Tensor>>(args[0])) {
        if (args.size() > 1) {
            Error::set(1, vm.runtime_current_line, "Dimensional reduction is not yet supported for Tensors in SUM.");
            return {};
        }
        return internal_tensor_sum(vm, args);
    }

    if (std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        // Your original array-based SUM logic (which handles dimensional reduction) can go here.
        // For now, we'll just call the simple scalar sum version.
        return internal_array_sum(vm, args);
    }

    Error::set(15, vm.runtime_current_line, "Argument to SUM must be an Array or a Tensor.");
    return {};
}


// ... (The full C++ implementation for all 12 builtin_... AI functions goes here,
//      e.g., builtin_to_tensor, builtin_conv2d, builtin_update, etc.)

// --- REGISTRATION ---

void register_ai_functions(NeReLaBasic& vm, NeReLaBasic::FunctionTable& table_to_populate) {
    // Helper lambda to make registration cleaner
    auto register_func = [&](const std::string& name, int arity, NeReLaBasic::NativeFunction func_ptr) {
        NeReLaBasic::FunctionInfo info;
        info.name = name;
        info.arity = arity;
        info.native_impl = func_ptr;
        table_to_populate[to_upper(info.name)] = info;
        };
    auto register_proc = [&](const std::string& name, int arity, NeReLaBasic::NativeFunction func_ptr) {
        NeReLaBasic::FunctionInfo info;
        info.name = name;
        info.arity = arity;
        info.native_impl = func_ptr;
        info.is_procedure = true;
        table_to_populate[to_upper(info.name)] = info;
        };

    register_func("SUM", -1, builtin_sum);

    // Register all AI functions with their "TENSOR." prefix
    register_func("TENSOR.FROM", 1, builtin_to_tensor);
    register_func("TENSOR.TOARRAY", 1, builtin_toarray);

    register_func("TENSOR.CREATE_LAYER", 2, builtin_create_layer);
    register_func("TENSOR.CREATE_OPTIMIZER", 2, builtin_create_optimizer);

    register_func("TENSOR.CONV2D", 5, builtin_conv2d);
    register_func("TENSOR.MAXPOOL2D", 3, builtin_maxpool2d);

    register_func("TENSOR.SIGMOID", 1, builtin_sigmoid);
    register_func("TENSOR.MATMUL", 2, builtin_matmul);

    register_proc("TENSOR.BACKWARD", 1, builtin_backward);
    register_func("TENSOR.UPDATE", 2, builtin_update);

    register_proc("TENSOR.SAVEMODEL", 2, builtin_save_model);
    register_func("TENSOR.LOADMODEL", 1, builtin_load_model);
}
