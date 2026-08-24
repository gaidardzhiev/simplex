int x = 99;
int *p = &x;
putint(*p);
putstr("\n");
*p = 42;
putint(x);
putstr("\n");
