LLVMCODE = optimizer

$(LLVMCODE): $(LLVMCODE).c
	g++ -g -c -I /usr/include/llvm-c-17/ $(LLVMCODE).c
	g++ $(LLVMCODE).o `llvm-config-17 --cxxflags --ldflags --libs core` -I /usr/include/llvm-c-17/ -o $@

clean: 
	rm -rf $(LLVMCODE)
	rm -rf *.o