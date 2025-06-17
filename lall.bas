' Traditional Functional/Procedural Approach for jdBasic
' ----------------------------------------------------

PRINT "--- Functional Style ---"

' A reusable function to test if a single number N is prime.
FUNC IS_PRIME(N)
    ' Numbers less than 2 are not prime.
    Ret = FALSE
    IF N < 2 THEN RETURN FALSE
    ' 2 is the only even prime.
    IF N = 2 THEN RETURN TRUE
    ' All other even numbers are not prime.
    IF N MOD 2 = 0 THEN RETURN FALSE

    ' Check for odd divisors from 3 up to the square root of N.
    LIMIT = SQR(N)
    FOR D = 3 TO LIMIT STEP 2
        IF N MOD D = 0 THEN
            Ret = FALSE
        ENDIF
    NEXT D
    ' If the loop finishes, no divisors were found. It's prime.
    RETURN TRUE
ENDFUNC

' Main loop: Test every number from 1 to 100 using our function.
PRINT "Primes up to 100:"
FOR I = 1 TO 100
    IF IS_PRIME(I) THEN
        PRINT I; " ";
    ENDIF
NEXT I
PRINT ""
