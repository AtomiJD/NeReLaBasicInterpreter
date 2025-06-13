print "Using higher order functions"
print

func inc(ab)
    return ab+1
endfunc
func dec(cd)
    return cd-1
endfunc

func apply(fa,cc)
    return fa(cc)
endfunc
print apply(inc@,10)
print apply(dec@,12)

print
print "Done"
print "------------------------"

