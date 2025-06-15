DIM deadline AS DATE
DIM name AS STRING

DIM intA As INTEGER

name = "Project Apollo"
deadline = NOW()

PRINT "Deadline for "; name; " is "; deadline

deadline = DATEADD("D", 10, deadline)
PRINT "Extended deadline is "; deadline

If deadline > Now() then
   print "Deadline is greater"
endif

PRINT "Current system date is "; DATE$()

intA = 5
print "A: "; intA
