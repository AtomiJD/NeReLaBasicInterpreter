PRINT "Start"

i = 0
fluppi:
i = i + 1

If i < 10 Then
        print i,"< 10"
Else
        print i,"> 10"
EndIf
If i > 20 Then
GoTo ende
EndIf

GoTo fluppi

ende:
print "finish"