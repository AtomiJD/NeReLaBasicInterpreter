REM -- Functional Map Example --
REM -- Applies a function to each element of an array.

DIM source_data[5]
DIM mapped_data[5]
source_data = [10, 20, 30, 40, 50]

REM This is the higher-order MAP function.
REM It takes an array and a "mapper" function as arguments.
FUNC map(source_array, dest_array, mapper_func@)
    FOR i = 0 TO 4
        dest_array[i] = mapper_func(source_array[i])
    NEXT i
ENDFUNC

REM --- Mapper Functions ---
REM These are simple functions that will be passed to MAP.

FUNC double_it(x)
    RETURN x * 2
ENDFUNC

FUNC add_five(x)
    RETURN x + 5
ENDFUNC

SUB print_array arr
    FOR i = 0 TO 4
        PRINT arr[i]
    NEXT i
    PRINT
ENDSUB

PRINT "--- Original Data ---"
print_array source_data

PRINT "--- Mapped with double_it ---"
r=map(source_data, mapped_data, double_it@)
print_array mapped_data

PRINT "--- Mapped with add_five ---"
r=map(source_data, mapped_data, add_five@)
print_array mapped_data
