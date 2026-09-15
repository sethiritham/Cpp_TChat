CXX := g++
CXXFLAGS := -std=c++17 -Iinclude
LDFLAGS := -lncurses -lsqlite3

SERVER := server
CLIENT := client

BCRYPT_SRCS := \
	src/bcrypt/bcrypt.cpp \
	src/bcrypt/blowfish.cpp

SERVER_SRC := src/ncurses_chat/participants/server2.cpp
CLIENT_SRC := src/ncurses_chat/participants/client2.cpp

SERVER_OBJS := $(SERVER_SRC:.cpp=.o) $(BCRYPT_SRCS:.cpp=.o)
CLIENT_OBJS := $(CLIENT_SRC:.cpp=.o) $(BCRYPT_SRCS:.cpp=.o)

.PHONY: all clean

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJS) -o $@ $(LDFLAGS)

$(CLIENT): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER) $(CLIENT) $(SERVER_OBJS) $(CLIENT_OBJS)
