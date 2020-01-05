# About

Work-in-progress compiler for a simple but powerful language.

Sample code snippet:

```
x + y = for y x +x
x - y = for y x -(+(-x))
x * y = for x z=0 (for y z +z)
         
(-x) abs = x
x abs = x

x == y = for (for y x x-1) 1 0

bool = true, false

// is the same as

bool 0
bool 1

true = bool 0
false = bool 1

(bool x) if do else = for x do else
```

This project uses makeheaders to automatically generate header files for each source file. Makeheaders is maintained [here](https://www.fossil-scm.org/fossil/file/src/makeheaders.c). Simply build it and put it in your path.