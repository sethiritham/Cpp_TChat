CXX      := g++
CXXFLAGS := -std=c++11 -Wall -O2 -Iinclude
LDFLAGS  :=

# ---- bcrypt example ----
BCRYPT_SRC_DIR := src/bcrypt
BCRYPT_SRCS    := $(BCRYPT_SRC_DIR)/Structures.cpp \
                   $(BCRYPT_SRC_DIR)/bcrypt.cpp \
                   $(BCRYPT_SRC_DIR)/blowfish.cpp \
                   $(BCRYPT_SRC_DIR)/bcrypt_example.cpp
BCRYPT_OBJS    := $(BCRYPT_SRCS:.cpp=.o)
BCRYPT_BIN     := bcrypt_example

# ---- ncurses chat ----
CHAT_SRC_DIR := src/ncurses_chat
CHAT_SRCS    := $(CHAT_SRC_DIR)/client.cpp \
                 $(CHAT_SRC_DIR)/main.cpp \
                 $(CHAT_SRC_DIR)/server.cpp
CHAT_OBJS    := $(CHAT_SRCS:.cpp=.o)
CHAT_BIN     := chat
CHAT_LDFLAGS := -lncurses -lpthread

.PHONY: all clean bcrypt chat

all: bcrypt chat

bcrypt: $(BCRYPT_BIN)

$(BCRYPT_BIN): $(BCRYPT_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

chat: $(CHAT_BIN)

$(CHAT_BIN): $(CHAT_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(CHAT_LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BCRYPT_OBJS) $(CHAT_OBJS) $(BCRYPT_BIN) $(CHAT_BIN)
