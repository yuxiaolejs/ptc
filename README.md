# Pi Theater Control (PTC)
The name... You know if you know.

Maybe a 340 project (if we have 340)...

Or just some random stuff...

Brain dump:
- Entry points: in host/xfb.c, pos/main.c
- GUI calls put pixel function provided by platform (host/xfb.c or pos/main.c)
- main funcs calls GUI renderer function
- Platforms will have their getkey() function, to poll, then handle in their internal functions (since they are different)