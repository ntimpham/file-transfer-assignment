SRC_DIR = src
OBJS_DIR = out
CXX = g++
DEBUG = -DDEBUG -g
CFLAGS = -std=c++11 -Wall -c
LFLAGS = -std=c++11 -Wall

CLIENT_SRC = client.o
SERVER_SRC = server.o
CLIENT_OBJS = $(addprefix $(OBJS_DIR)/, $(CLIENT_SRC))
SERVER_OBJS = $(addprefix $(OBJS_DIR)/, $(SERVER_SRC))

$(OBJS_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) $(CFLAGS) $< -o $@

all : execs

debug : CFLAGS += $(DEBUG)
debug : LFLAGS += $(DEBUG)
debug : execs

execs : directories client_bin server_bin scripts

client_bin : $(CLIENT_OBJS)
	$(CXX) $(LFLAGS) $(CLIENT_OBJS) -o client_bin

server_bin : $(SERVER_OBJS)
	$(CXX) $(LFLAGS) $(SERVER_OBJS) -o server_bin

directories :
	mkdir -p $(OBJS_DIR)

scripts :
	chmod +x client server

.PHONY : clean

clean :
	rm -rf out/ client_bin server_bin port a3.tar

tar :
	tar -cf a3.tar client server src/ makefile README
