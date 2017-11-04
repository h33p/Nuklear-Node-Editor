# Install
BIN = demo

# This is what works for me, on a mac using homebrew
CINCLUDE_PATH=-I /usr/local/Cellar/glew/1.13.0/include -I /usr/local/Cellar/glfw3/3.1.2/include

# Flags
CFLAGS = -O3

SRC = main.cpp
OBJ = $(SRC:.cpp=.o)

ifeq ($(OS),Windows_NT)
BIN := $(BIN).exe
LIBS = -lglfw3 -lopengl32 -lm -lGLU32 -lGLEW32
else
	UNAME_S := $(shell uname -s)
	GLFW3 := $(shell pkg-config --libs glfw3)
	ifeq ($(UNAME_S),Darwin)
		LIBS := $(GLFW3) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm -lGLEW -L/usr/local/lib
	else
		LIBS = $(GLFW3) -lGL -lm -lGLU -lGLEW
	endif
endif

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CXX) $(SRC) $(CFLAGS) -o bin/$(BIN) $(CINCLUDE_PATH) $(LIBS)
