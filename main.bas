' Save this file as main.bas

' Import the MATH module. The interpreter will look for "MATH.bas"
' and compile it before running this program.
import MATH

print "--- Module Test Program ---"
print

' Call the exported 'add' function from the MATH module
print "Calling MATH.ADD(15, 7)..."
x = MATH.ADD(15, 7)
print "Result:"; x
print

' Call the exported 'add_and_double' function
print "Calling MATH.ADD_AND_DOUBLE(10, 5)..."
y = MATH.ADD_AND_DOUBLE(10, 5)
print "Result:"; y
print

' Call the exported 'print_sum' procedure
print "Calling MATH.PRINT_SUM 100, 23..."
MATH.PRINT_SUM 100, 23
print

print "--- Module Test Complete ---"
