# jdBasic - The Neo Retro Lambda BASIC Interpreter

 **jdBasic** (also known as **NeReLa BASIC** for **Ne**o **Re**tro **La**mbda) is not your grandfather's BASIC. It's a modern, powerful interpreter that blends the simplicity and familiarity of classic BASIC with core concepts from functional programming.

Whether you're looking to relive the nostalgia of 8-bit coding with modern conveniences or explore functional paradigms in a simple syntax, jdBasic offers a unique and powerful environment. The language is designed to be easy to learn but capable enough to build complex, modular applications.

## Key Features

jdBasic is packed with features that bridge the gap between retro and modern programming:

#### Classic BASIC Foundations

  * Familiar control flow with `FOR...NEXT` loops, `GOTO`, and labels.
  * Multi-line `IF...THEN...ELSE...ENDIF` blocks for conditional logic.
  * Single-line `IF...THEN` for concise checks.
  * A rich library of built-in functions for string manipulation (`TRIM$`, `LEFT$`, `MID$`, `INSTR`, etc.) and math (`SQR`, `SIN`, `RND`, etc.).

#### Modern Enhancements

  * **Modular Programming**: Organize your code into reusable modules with `IMPORT`.
  * **Typed Variables**: Declare variables with types like `DATE` for better code clarity and safety.
  * **Rich Data Types**: First-class support for Arrays and Date/Time objects.
  * **Array Literals**: Initialize arrays with a simple, clean syntax (e.g., `my_array = [1, 2, 3]`).

#### Functional Programming Core (The "Lambda" in NeReLa)

  * **First-Class Functions**: Treat functions as values. Assign them to variables and pass them to other functions.
  * **Higher-Order Functions**: Create functions that take other functions as arguments, enabling powerful patterns like `map` and `filter`.
  * **Recursion**: Write elegant, recursive functions for tasks like calculating factorials or traversing data structures.
  * **Clean Function/Procedure Syntax**: Easily define multi-line functions (`FUNC...ENDFUNC`) and procedures (`SUB...ENDSUB`).

## Getting Started

To run a jdBasic program, simply pass the source file to the interpreter from your command line:

```sh
# Replace 'jdBasic' with the actual executable name on your system
jdBasic test.bas
```

## Language Tour: A Look at the Syntax

The following examples are taken from `test.bas` and demonstrate the core capabilities of the language.

### Looping and Control Flow

Classic `FOR...NEXT` loops and `GOTO` statements are fully supported for simple iteration and control.

```basic
' Classic For...Next loop
for i = 1 to 5
    print "lall: "; i
next i

' If/Then/Else with Goto
i = 0
fluppi:
i = i + 1
print i;

if i > 20 then
    goto ende
else
    print "luli"
endif

GoTo fluppi

ende:
print "done"
```

### Working with Arrays

Declare, access, and even initialize arrays with a modern literal syntax.

```basic
PRINT "Array Test"

' Declare an array of 20 elements
Dim a[20]

' Assign values using a loop
for i = 0 to 19
    a[i] = i*2
next i

' Initialize arrays directly with literal values
DIM my_numbers[10]
DIM my_strings$[5]

my_numbers = [10, 20, 30, 40, 50]
my_strings$ = ["alpha", "beta", "gamma"]

PRINT "--- Strings ---"
FOR i = 0 TO 4
    PRINT my_strings$[i]
NEXT i
```

### Date and Time

jdBasic has built-in support for date/time manipulation.

```basic
PRINT "--- DATE Test ---"
Print "Tick: "; Tick()
Print "Now: "; Now()

DIM deadline AS DATE
deadline = CVDate("2025-07-01") ' Convert string to Date type

PRINT "Deadline is "; deadline

deadline = DATEADD("D", 10, deadline) ' Add 10 days
PRINT "Extended deadline is "; deadline

If deadline > Now() then
    print "Deadline is in the future"
endif
```

### Modular Programming

Import code from other files to keep your projects clean and organized.

```basic
' Import the MATH module. The interpreter will look for "MATH.bas"
import MATH

' Call an exported function from the module
print "Calling MATH.ADD(15, 7)..."
x = MATH.ADD(15, 7)
print "Result:"; x

' Call an exported procedure from the module
print "Calling MATH.PRINT_SUM 100, 23..."
MATH.PRINT_SUM 100, 23
```

### Functional Programming: The Heart of jdBasic

This is where jdBasic truly shines. Functions are first-class citizens.

#### Higher-Order Functions

The `@` operator creates a reference to a function, allowing it to be passed as an argument.

```basic
' Simple functions to be used as arguments
func inc(ab)
    return ab+1
endfunc
func dec(ac)
    return ac-1
endfunc

' A function that takes another function as an argument
func apply(fa, cc)
    return fa(cc) ' Execute the passed-in function
endfunc

' Pass the 'inc' and 'dec' functions to 'apply'
print apply(inc@, 10)  ' Prints 11
print apply(dec@, 12)  ' Prints 11
```

#### Recursion

Easily write recursive functions.

```basic
' Classic factorial example
func factorial(a)
    if a > 1 then
        return factorial(a-1) * a
    else
        return a
    endif
endfunc

lall =  factorial(5)
print "Factorial of 5 is: ", lall ' Prints 120
```

#### Writing a `filter` function

You can easily implement powerful functional helpers like `filter` right in jdBasic.

```basic
' A function that returns 1 if a number is even, 0 otherwise
func iseven(a)
    if a mod 2 = 0 then
        return 1
    else
        return 0
    endif
endfunc

' A generic filter procedure
' It takes a predicate function (fu) and applies it to an input array (in[]),
' placing matching items into an output array (out[]).
func filter(fu, in[], out[])
    j=0
    for i = 0 to len(in) - 1
        m=fu(in[i])
        if m = 1 then
            out[j]=in[i]
            j=j+1
        endif
    next i
endfunc

' --- Usage ---
Dim source_data[20]
Dim filtered_data[20]
source_data = [1, 5, 8, 7, 45, 66, 12]

' Filter the source data using the 'iseven' function
filter(iseven@, source_data[], filtered_data[])

print "Even numbers: "
' (Code to print the filtered_data array)
```

## Future Roadmap

jdBasic is an active project. Future plans include:

  * Expanding the standard library with more modules (file I/O, graphics).
  * Adding more data structures (maps, dictionaries).
  * Improving error reporting with more descriptive messages.
  * Cross-platform builds.

Contributions and feedback are welcome\!
