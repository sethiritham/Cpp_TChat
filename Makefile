
CXX := g++

# ==========================================
# Include paths
# ==========================================

CPPFLAGS += -Iinclude
CPPFLAGS += -Iinclude/bcrypt
CPPFLAGS += -Iinclude/parser

# Homebrew OpenSSL (macOS)
OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null)

ifneq ($(strip $(OPENSSL_PREFIX)),)
    CPPFLAGS += -I$(OPENSSL_PREFIX)/include
    LDFLAGS += -L$(OPENSSL_PREFIX)/lib
endif


# ==========================================
# Compiler options
# ==========================================

CXXFLAGS += -std=c++17
CXXFLAGS += -Wall -Wextra -Wpedantic
CXXFLAGS += -g -O0


# ==========================================
# Libraries
# ==========================================

COMMON_LIBS := -lncurses -lssl -lcrypto

SERVER_LIBS := $(COMMON_LIBS) -lsqlite3
CLIENT_LIBS := $(COMMON_LIBS)


# ==========================================
# Build directories
# ==========================================

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

SERVER := $(BUILD_DIR)/server
CLIENT := $(BUILD_DIR)/client


# ==========================================
# Shared source files
# ==========================================

COMMON_SRC := \
	src/common/network.cpp \
	src/common/protocol.cpp \
	src/common/ui.cpp


# ==========================================
# Bcrypt source files
# Server only
# ==========================================

BCRYPT_SRC := \
	src/bcrypt/bcrypt.cpp \
	src/bcrypt/blowfish.cpp


# ==========================================
# Server source files
# ==========================================

SERVER_SRC := \
	src/server/main.cpp \
	src/server/app.cpp \
	src/server/authorization.cpp \
	src/server/manager.cpp \
	src/server/io.cpp \
	src/server/message_handler.cpp \
	src/server/sql_helper.cpp \
	src/server/event_handler.cpp \
	$(COMMON_SRC) \
	$(BCRYPT_SRC)


# ==========================================
# Client source files
# ==========================================

CLIENT_SRC := \
	src/client/main.cpp \
	src/client/app.cpp \
	src/client/authorization.cpp \
	src/client/connection.cpp \
	src/client/io.cpp \
	src/client/message_handler.cpp \
	$(COMMON_SRC)


# ==========================================
# Object file mapping
# ==========================================

SERVER_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRC))

CLIENT_OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CLIENT_SRC))

DEPFILES := $(sort \
	$(SERVER_OBJS:.o=.d) \
	$(CLIENT_OBJS:.o=.d) \
)


# ==========================================
# Targets
# ==========================================

.PHONY: all server client clean

all: server client

server: $(SERVER)

client: $(CLIENT)


# ==========================================
# Linking
# ==========================================

$(SERVER): $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SERVER_LIBS)

$(CLIENT): $(CLIENT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(CLIENT_LIBS)


# ==========================================
# Compilation
# ==========================================

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@


# ==========================================
# Cleaning
# ==========================================

clean:
	rm -rf $(BUILD_DIR)


# ==========================================
# Header dependencies
# ==========================================

-include $(DEPFILES)
