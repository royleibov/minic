extern void print(int);
extern int read();

int func(int i){
	int a;
	int b;
	
	b = 0;
	while (b < i){
		int a;
		int d;
		a = read();
		b = 10 + a;

		d = b / 2;
        if (b == d)
            print(b);
        else
            print(a);

	}
	return(b);
}
