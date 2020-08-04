

TC_MALLOC_PATH=tcmalloc_2/tcmalloc_lib/
TC_LD_LIB=$(TC_MALLOC_PATH)/lib/
TC_MALLOC_LDFLAGS = -L$(TC_LD_LIB) -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -ltcmalloc -Wl,-R$(TC_LD_LIB)


all:
	g++ -O3 -std=c++17 test.cc -o test  -lpthread
	g++ -O3 -std=c++17 obj_slab_test.cc -o test_os -lpthread -march=native
	g++ -O3 -std=c++17 obj_slab_test.cc -DSUPER_SLAB -o test_os_ss -lpthread -march=native	
	g++ -O3 -std=c++17 obj_slab_test.cc -DSTD_MALLOC -o test_os_std -lpthread -march=native	
	g++ -O3 -std=c++17 -DSTD_MALLOC obj_slab_test.cc  -o test_os_tc  $(TC_MALLOC_LDFLAGS) -lpthread -march=native
clean:
	rm -f *~ *#* test test_os_std test_os_tc test_os test_os_ss
