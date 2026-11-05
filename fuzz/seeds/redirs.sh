echo a > f; echo b >> f; echo c >| f; cat < f 2>&1; exec 3>f 4<&0; echo x <> f
