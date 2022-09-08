CC=g++

restorelist: restorelist.cpp
	$(CC) -o restorelist restorelist.cpp -O2 -std=c++20 -lmariadbcpp

clean:
	rm -f ./restorelist
