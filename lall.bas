print "Using higher order functions"
print

func inc(ab)
    print "in inc function: ab: "; ab
    return ab+1
endfunc

func dec(la)
    print "in dec function: cd: "; la
    return la-1
endfunc

func apply(fa,cc)
    print "cc: ";cc
    a = fa(cc)
    print "fa(cc)="; a
    return a
endfunc

print apply(inc@,10)
print apply(dec@,12)

print
print "Done"


