CXX := g++

# Header search paths
CPPFLAGS += -Iinclude
CPPFLAGS += -I/opt/homebrew/opt/openssl@3/include

# Compiler options
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic

# Linker search paths
LDFLAGS += -L/opt/homebrew/opt/openssl@3/lib

# Libraries
LDLIBS += -lncurses -lsqlite3 -lssl -lcrypto

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

SERVER := $(BUILD_DIR)/server
CLIENT := $(BUILD_DIR)/client

# Shared implementation files
COMMON_SRC := \
	src/common/network.cpp \
	src/common/protocol.cpp \
	src/common/ui.cpp

BCRYPT_SRC := \
	src/bcrypt/bcrypt.cpp \
	src/bcrypt/blowfish.cpp

# Refactored server
SERVER_SRC := \
	src/server/main.cpp \
	src/server/app.cpp \
	src/server/authorization.cpp \
	src/server/connection.cpp \
	src/server/io.cpp \
	src/server/message_handler.cpp \
	src/server/sql_helper.cpp \
	$(COMMON_SRC) \
	$(BCRYPT_SRC)

# Existing client
CLIENT_SRC := \
	src/participants/client2.cpp \
	$(COMMON_SRC) \
	$(BCRYPT_SRC)

# Map source paths to build/obj paths
SERVER_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRC))
CLIENT_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CLIENT_SRC))

DEPFILES := $(sort $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d))

.PHONY: all clean

all: $(SERVER) $(CLIENT)

server: $(SERVER)
client: $(CLIENT)

$(SERVER): $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(CLIENT): $(CLIENT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile source files and generate header dependencies
$(OBJ_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPFILES)
