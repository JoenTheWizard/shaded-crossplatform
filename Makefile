FLG=-lGL -lX11 -lpthread -lXrandr -lXi -ldl -lportaudio
GLAD=-I glad/include
DEBUG=-g3
WARNINGS=-Wall -Wextra
INCLUDES=includes/*.cpp mlp_nn/mlp_nn.c mlp_nn/matrix.c
OUTPUT=shaded
#Compile
all:
	g++ $(GLAD) `pkg-config --cflags glfw3` -o $(OUTPUT)  main.cpp glad/src/glad.c `pkg-config --libs glfw3` $(FLG)

clean:
	rm shaded
