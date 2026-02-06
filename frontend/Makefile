CXX = g++
CXXFLAGS = -g -std=c++11 -x c++

all: y.tab.o lex.yy.o ast.o frontend.o

y.tab.c y.tab.h: parser.y
	yacc -d -v parser.y

lex.yy.c: parser.l
	lex parser.l

y.tab.o: y.tab.c y.tab.h ast.h frontend.h
	$(CXX) $(CXXFLAGS) -c y.tab.c -o y.tab.o

lex.yy.o: lex.yy.c y.tab.h ast.h
	$(CXX) $(CXXFLAGS) -c lex.yy.c -o lex.yy.o

ast.o: ast.c ast.h
	$(CXX) $(CXXFLAGS) -c ast.c -o ast.o

frontend.o: frontend.c frontend.h ast.h
	$(CXX) $(CXXFLAGS) -c frontend.c -o frontend.o

clean:
	rm -f y.tab.c y.tab.h lex.yy.c y.output *.o
