x="a b"; y='c $x'; echo "${x} ${y:-d} ${#x} ${x%b} ${x#a} $(echo sub) `echo bt` $((x+1))"
