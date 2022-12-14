#
# 'make'        build executable file 'main'
# 'make clean'  removes all .o and executable files
#

# define the Cpp compiler to use
CXX = g++

# define any compile-time flags
CXXFLAGS	:= -std=c++17 -Wall -Wextra -g

# define library paths in addition to /usr/lib
#   if I wanted to include libraries not in /usr/lib I'd specify
#   their path using -Lpath, something like:
LFLAGS = -lpthread

rebuild: clean all
rebuild_client: clean client
rebuild_server: clean server
server: output/server
client: output/client
all: server client

clean:
	clear
	rm -f output/* obj/*

obj/server.o: src/server.cpp
	mkdir -p obj
	$(CXX) $(CXXFLAGS) -o obj/server.o src/server.cpp -c

obj/client.o: src/client.cpp
	mkdir -p obj
	$(CXX) $(CXXFLAGS) -o obj/client.o src/client.cpp -c

output/server: obj/server.o
	mkdir -p output
	$(CXX) $(CXXFLAGS) -o output/server obj/server.o $(LFLAGS)

output/client: obj/client.o
	mkdir -p output
	$(CXX) $(CXXFLAGS) -o output/client obj/client.o $(LFLAGS)