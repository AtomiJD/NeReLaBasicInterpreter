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
