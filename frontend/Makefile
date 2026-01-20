CXX = g++
CXXFLAGS = -g -std=c++11

all: parser

parser: parser.l parser.y ast.c ast.h
	yacc -d -v parser.y
	lex parser.l
	$(CXX) $(CXXFLAGS) -x c++ y.tab.c lex.yy.c ast.c -o parser

clean:
	rm -f parser y.tab.c y.tab.h lex.yy.c y.output