Print "Tick: "; Tick()
Print "Now: "; Now()

DIM deadline AS DATE
DIM name AS STRING

name = "Project Apollo"
deadline = CVDate("2025-07-01")

PRINT "Deadline for "; name; " is "; deadline

deadline = DATEADD("D", 10, deadline)
PRINT "Extended deadline is "; deadline

If deadline > Now() then
   print "Deadline is greater"
endif
