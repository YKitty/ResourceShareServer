all:httpserver upload
httpserver:httpserver.cpp utils.hpp
	g++ -o $@ $^ -l pthread -std=c++11

upload:upload.cpp 
	g++ -o $@ $^ -std=c++11

.PHONY:clean
clean:
	rm httpserver -f
