Dim o_map[20]

s_map = [1,5,8,7,45,66,12]

sub printresult(result)
   for i = 0 to len(result)-1
       if result[i]>0 then
           print result[i], " ";
       endif
   next i
print
endsub

func iseven(a)
   if a mod 2 = 0 then
      r=1
   else
      r=0
   endif
   return r
endfunc

func filter(fu,in[],out[])
   j=0
   for i = 0 to len(in) - 1
      m=fu(in[i])
      if m = 1 then
         out[j]=in[i]
         j=j+1
      endif
   next i
endfunc

filter(iseven@,s_map,o_map)

print "Result filter: "
printresult o_map
