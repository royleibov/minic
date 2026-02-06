CXX = g++
CXXFLAGS = -g -std=c++11 -x c++

FRONTEND_OBJS = frontend/y.tab.o frontend/lex.yy.o frontend/ast.o frontend/frontend.o

all: minic_parser

$(FRONTEND_OBJS):
	$(MAKE) -C frontend

minic_parser: minic_parser.c $(FRONTEND_OBJS)
	$(CXX) $(CXXFLAGS) minic_parser.c -x none $(FRONTEND_OBJS) -o minic_parser

clean:
	$(MAKE) -C frontend clean
	rm -f minic_parser
