Print "For Next Test"

goto funci

for i = 1 to 5
 print "lall: "; i
next i

print "done"
print "------------------------"

Print "Nested For Next Test"

for i = 1 to 5
    for j = 1 to 5
        print "lall: "; i*j
    next j
    print
next i

print "done"
print "------------------------"

PRINT "Goto/Label and if then else Test"

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
print "------------------------"

PRINT "Array Test"

Dim a[20]

for i = 0 to 19
  a[i] = i*2
next i

for i = 0 to 19
  print "Zahl #",i, " ist: ", a[i]
next i

DIM my_numbers[10]
DIM my_strings$[5]

my_numbers = [10, 20, 30, 40, 50]
my_strings$ = ["alpha", "beta", "gamma"]

PRINT "--- Numbers ---"
FOR i = 0 TO 9
    PRINT my_numbers[i]
NEXT i

PRINT "--- Strings ---"
FOR i = 0 TO 4
    PRINT my_strings$[i]
NEXT i

print "done"
print "------------------------"

PRINT "--- String and Math Function Test ---"

GREETING$ = "   Hello, World!   "
PRINT "Original: '"; GREETING$; "'"
PRINT "Trimmed: '"; TRIM$(GREETING$); "'"
PRINT "Length of trimmed: "; LEN(TRIM$(GREETING$))
PRINT

PRINT "LEFT 5: "; LEFT$(TRIM$(GREETING$), 5)
PRINT "RIGHT 6: "; RIGHT$(TRIM$(GREETING$), 6)
PRINT "MID from 8 for 5: "; MID$(TRIM$(GREETING$), 8, 5)

PRINT "LOWER: "; LCASE$("THIS IS A TEST")
PRINT "UPPER: "; UCASE$("this is a test")
PRINT

PRINT "ASCII for 'A' is "; ASC("A")
PRINT "Character for 66 is "; CHR$(66)
PRINT

HAYSTACK$ = "the quick brown fox jumps over the lazy dog"
PRINT "Position of 'fox' is "; INSTR(HAYSTACK$, "fox")
PRINT "Position of 'the' after pos 5 is "; INSTR(5, HAYSTACK$, "the")
PRINT

PRINT "--- Math Test ---"
X = 90
PRINT "The square root of ";X;" is "; SQR(X)

' Note: SIN/COS/TAN work in radians, not degrees
PI = 3.14159
PRINT "The sine of PI/2 is roughly "; SIN(PI / 2)

PRINT "--- String and Math Function Test COMPLETE ---"


funci:
Print "Functional tests"
print
Print "Function Definition and call functions"

func lall(a,b)
    return a*b
endfunc

print "func call:"
b=lall(5,3)
print b

print
print "Done"
print "------------------------"

print "Using higher order functions"
print

func inc(ab)
    return ab+1
endfunc
func dec(ac)
    return ac-1
endfunc

func apply(fa,cc)
    return fa(cc)
endfunc
print apply(inc@,10)
print apply(dec@,12)

print
print "Done"
print "------------------------"

print "Simple recursion"
print

func factorial(a)
    if a > 1 then
        return factorial(a-1)*a
    else
        return a
    endif
endfunc

lall =  factorial(5)
print "erg: ", lall

print
print "Done"
print "------------------------"

print "Simple recursion"
print

func printnumbers(n)
    if n > 0 then
        print n
	r=printnumbers(n - 1)
    endif
endfunc

r=printnumbers(5)  ' returns 5, 4, 3, 2, 1
