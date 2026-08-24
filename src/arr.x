int buf0 = 0;
int buf1 = 0;
int buf2 = 0;
int buf3 = 0;

int *p = &buf0;
*p = 10;
p[1] = 20;
p[2] = 30;
p[3] = 40;

putint(p[0]);
putstr("\n");
putint(p[1]);
putstr("\n");
putint(p[2]);
putstr("\n");
putint(p[3]);
putstr("\n");
