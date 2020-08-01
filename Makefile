


all:
	g++ -O3 test.cc -o test  -lpthread
	g++ -O3 -ggdb obj_slab_test.cc -o test_os  -lpthread -march=native
clean:
	rm -f *~ *#* test test_os
