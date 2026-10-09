int x = 8;
int *p = &x;
putint(*p);
putstr("\n");
*p = 16;
putint(x);
putstr("\n");
