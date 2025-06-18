Of course. Creating a comprehensive reference manual is a great way to solidify the project. Based on the source files you've provided, here is a `README.md` file documenting all the available non-graphics commands and functions in jdBasic, with examples drawn from your `test.txt` file.

-----

# jdBasic Language Reference

This document serves as the official language reference for the jdBasic interpreter. It details all available statements, functions, and operators.

## Table of Contents

1.  [Commands (Statements)](https://www.google.com/search?q=%23commands-statements)
2.  [Built-in Functions](https://www.google.com/search?q=%23built-in-functions)
      * [String Functions](https://www.google.com/search?q=%23string-functions)
      * [Math Functions](https://www.google.com/search?q=%23math-functions)
      * [Array & Matrix Functions (APL-Style)](https://www.google.com/search?q=%23array--matrix-functions-apl-style)
      * [Date & Time Functions](https://www.google.com/search?q=%23date--time-functions)
      * [File I/O Functions](https://www.google.com/search?q=%23file-io-functions)
3.  [Operators](https://www.google.com/search?q=%23operators)
4.  [Error Codes](https://www.google.com/search?q=%23error-codes)

-----

## Commands (Statements)

These are the core keywords that control program flow and actions.

  * **`CD path$`**
    > Changes the current working directory.
    ```basic
    CD "C:\Users\Public"
    ```
  * **`CLS`**
    > Clears the text console screen.
  * **`COLOR fg, bg`**
    > Sets the foreground and background colors of the text console.
  * **`COMPILE`**
    > Compiles the source code currently in memory into p-code.
  * **`CURSOR state`**
    > Sets the visibility of the console cursor. `CURSOR 1` shows it, `CURSOR 0` hides it.
  * **`DIM var[size, ...]` or `DIM var AS type`**
    > Declares a multi-dimensional array or a variable with a specific type (`INTEGER`, `STRING`, `DOUBLE`, `DATE`).
    ```basic
    DIM a[20]
    DIM deadline AS DATE
    ```
  * **`DIR [path_with_wildcard$]`**
    > Lists the contents of a directory. Supports `*` and `?` wildcards.
    ```basic
    DIR "*.txt"
    ```
  * **`DUMP [arg]`**
    > Dumps internal state. `DUMP` shows p-code, `DUMP "GLOBAL"` shows global variables, `DUMP "LOCAL"` shows local variables.
  * **`EDIT ["filename"]`**
    > Opens a full-screen text editor for the specified file or the code currently in memory.
  * **`FOR var = start TO end [STEP s]` ... `NEXT`**
    > Creates a loop that increments a variable.
    ```basic
    FOR I = 1 TO 10 STEP 2
      PRINT I
    NEXT I
    ```
  * **`FUNC name(params)` ... `ENDFUNC`**
    > Defines a function that can return a value.
    ```basic
    FUNC ADD(A, B)
        RETURN A + B
    ENDFUNC
    ```
  * **`GOTO label`**
    > Jumps program execution to a line with the specified `label:`.
  * **`IF condition THEN ... [ELSE ...] ENDIF`**
    > Executes code conditionally. Supports both multi-line blocks and single-line statements.
    ```basic
    IF I > 20 THEN
       GOTO ende
    ELSE
       PRINT "luli"
    ENDIF

    [cite_start]IF I = 5 THEN PRINT "FIVE" [cite: 23]
    ```
  * **`IMPORT module`**
    > Imports an external `.jdb` module, making its exported functions and subs available.
    ```basic
    IMPORT MATH
    [cite_start]X = MATH.ADD(15, 7) [cite: 3]
    ```
  * **`INPUT [prompt$ ,|;] var`**
    > Pauses and waits for the user to enter data, which is stored in `var`.
  * **`KILL filename$`**
    > Deletes a file.
  * **`LIST`**
    > Displays the source code currently in memory.
  * **`LOAD filename$`**
    > Loads a source file from disk into memory.
  * **`LOCATE row, col`**
    > Positions the text cursor at a specific row and column on the console.
  * **`MKDIR path$`**
    > Creates a new directory.
  * **`PRINT expr [,|;] ...`**
    > Prints the value of an expression to the screen. A trailing comma or semicolon suppresses the newline.
  * **`PWD`**
    > Prints the current working directory.
  * **`REM` or `'`**
    > Marks the rest of the line as a comment, which is ignored by the interpreter.
  * **`RETURN [value]`**
    > Exits a `FUNC` and returns an optional `value`.
  * **`RUN`**
    > Compiles and runs the source code currently in memory.
  * **`SAVE filename$`**
    > Saves the source code currently in memory to a file on disk.
  * **`SLEEP ms`**
    > Pauses program execution for a specified number of milliseconds.
  * **`STOP`** & **`RESUME`**
    > `STOP` halts execution and enters a break state. `RESUME` continues execution from where it stopped.
  * **`SUB name(params)` ... `ENDSUB`** & **`CALLSUB name, params`**
    > Defines and calls a procedure that does not return a value.
  * **`TRON`** & **`TROFF`**
    > Turns execution tracing on and off for debugging.

-----

## Built-in Functions

### String Functions

  * [cite\_start]**`ASC(text$)`**: Returns the ASCII code of the first character in a string. [cite: 11]
  * [cite\_start]**`CHR$(n)`**: Returns a one-character string corresponding to an ASCII code. [cite: 11]
  * **`INKEY$`**: Checks for a keypress without pausing. Returns the key pressed or an empty string.
  * [cite\_start]**`INSTR([start,] haystack$, needle$)`**: Finds the position of `needle$` within `haystack$`. [cite: 13, 14]
  * [cite\_start]**`LCASE$(text$)`**: Converts a string to all lowercase. [cite: 10]
  * **`LEFT$(text$, n)`**: Returns the first `n` characters of a string.
  * **`LEN(text$)`**: Returns the number of characters in a string.
  * [cite\_start]**`MID$(text$, start, [length])`**: Returns a substring starting from `start` with an optional `length`. [cite: 9]
  * [cite\_start]**`RIGHT$(text$, n)`**: Returns the last `n` characters of a string. [cite: 9]
  * **`SPLIT(source$, delimiter$)`**: Returns a 1D array of strings by splitting `source$` at each `delimiter$`.
  * [cite\_start]**`STR$(n)`**: Converts a number `n` into its string representation. [cite: 12]
  * [cite\_start]**`TRIM$(text$)`**: Removes leading and trailing whitespace from a string. [cite: 8]
  * [cite\_start]**`UCASE$(text$)`**: Converts a string to all uppercase. [cite: 10]
  * [cite\_start]**`VAL(text$)`**: Converts a string representation of a number into a numeric type. [cite: 12]

### Math Functions

  * **`COS(n)`**: Returns the cosine of `n` (in radians).
  * [cite\_start]**`RND(n)`**: Returns a random floating-point number between 0.0 and 1.0. [cite: 17]
  * [cite\_start]**`SIN(n)`**: Returns the sine of `n` (in radians). [cite: 16]
  * [cite\_start]**`SQR(n)`**: Returns the square root of `n`. [cite: 15]
  * **`TAN(n)`**: Returns the tangent of `n` (in radians).

### Array & Matrix Functions (APL-Style)

  * **`ALL(array)`**: Returns `TRUE` if all elements in the array are true.
  * **`ANY(array)`**: Returns `TRUE` if any element in the array is true.
  * **`APPEND(array, value)`**: Returns a new array with `value` (or all elements of an array `value`) appended to the end.
  * **`DIFF(array1, array2)`**: Returns a new array with elements that are in `array1` but not in `array2`.
  * **`DROP(n, array)`**: Returns a new array with the first `n` (or last `n` if negative) elements removed.
  * **`GRADE(array)`**: Returns an array of indices that would sort the input array.
  * **`IOTA(n)`**: Creates a 1D array of `n` numbers, from 1 to `n`.
  * [cite\_start]**`LEN(array)`**: Returns a 1D array containing the shape (dimensions) of the input array. [cite: 5]
  * **`MATMUL(matrixA, matrixB)`**: Performs standard matrix multiplication.
  * **`MAX(array)`**: Returns the largest numeric value in an array.
  * **`MIN(array)`**: Returns the smallest numeric value in an array.
  * **`OUTER(arrayA, arrayB, op$)`**: Creates a new, higher-dimension array by applying the string operator `op$` to every pair of elements from the input arrays.
  * **`PRODUCT(array)`**: Returns the product of all elements in an array.
  * **`RESHAPE(array, shape_vector)`**: Returns a new array with the same data but with new dimensions specified by `shape_vector`.
  * **`REVERSE(array)`**: Reverses the elements of an array along its last dimension.
  * **`SLICE(matrix, dimension, index)`**: Extracts a 1D vector (a row or column) from a 2D matrix.
  * **`SUM(array)`**: Returns the sum of all elements in an array.
  * **`TAKE(n, array)`**: Returns a new array with the first `n` (or last `n` if negative) elements.
  * **`TRANSPOSE(matrix)`**: Swaps the rows and columns of a 2D matrix.

### Date & Time Functions

  * [cite\_start]**`CVDATE(date$)`**: Converts a string in "YYYY-MM-DD" format to a `DATE` type value. [cite: 19]
  * **`DATE$`**: Returns the current system date as a string (e.g., "2025-06-18").
  * [cite\_start]**`DATEADD(part$, n, date)`**: Adds `n` units of `part$` (e.g., "D" for day) to a `DATE` value. [cite: 19]
  * [cite\_start]**`NOW()`**: Returns a `DATE` type value representing the current date and time. [cite: 18]
  * [cite\_start]**`TICK()`**: Returns the number of milliseconds since the program started. [cite: 18]
  * **`TIME$`**: Returns the current system time as a string (e.g., "15:00:00").

### File I/O Functions

  * **`CSVREADER(filename$, [delimiter$], [has_header])`**: Reads a numerical CSV file into a 2D matrix.
  * **`TXTREADER$(filename$)`**: Reads an entire text file into a single string.

-----

## Operators

  * **Arithmetic**: `+`, `-`, `*`, `/`, `MOD`. These operate element-wise when used with arrays.
  * **Comparison**: `=`, `<>`, `<`, `>`, `<=`, `>=`.
  * **Logical**: `AND`, `OR`, `NOT`.
  * **Function Reference**: `@`. [cite\_start]Used after a function name to create a reference that can be passed to another function (e.g., `apply(inc@, 10)` [cite: 22]).

-----

## Error Codes

| Code | Message |
| :--- | :--- |
| 0 | OK |
| 1 | Syntax Error |
| 2 | Calculation Error |
| 3 | Variable not found |
| 4 | Unclosed IF/ENDIF |
| 5 | Unclosed FUNC/ENDFUNC |
| 6 | File not found |
| 7 | Function/Sub name not found |
| 8 | Wrong number of arguments |
| 9 | RETURN without GOSUB/CALL |
| 10 | Array out of bounds |
| 11 | Undefined label |
| 12 | File I/O Error |
| 13 | Invalid token in expression |
| 14 | Unclosed loop |
| 15 | Type Mismatch |
| 21 | NEXT without FOR |
| 22 | Undefined function |
| 23 | RETURN without function call |
| 24 | Bad array subscript |
| 25 | Function or Sub is missing RETURN or END |
| 26 | Incorrect number of arguments |