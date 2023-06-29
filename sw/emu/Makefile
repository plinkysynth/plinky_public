#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need GLFW (http://www.glfw.org):
# Linux:
#   apt-get install libglfw-dev
# Mac OS X:
#   brew install glfw
# MSYS2:
#   pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
#

#CXX = g++
#CXX = clang++

IMGUI_DIR = imgui
PORTAUDIO_DIR = portaudio

BASEOBJDIR = obj
BASETARGETDIR = bin

ifeq "$(strip $(filter debug,$(MAKECMDGOALS)))" "debug"
OBJDIR = $(BASEOBJDIR)/debug
TARGETDIR = $(BASETARGETDIR)/debug
else
OBJDIR = $(BASEOBJDIR)/release
TARGETDIR = $(BASETARGETDIR)/release
endif

EXENAME = plinky
EXE = $(TARGETDIR)/$(EXENAME)

SOURCES = main.cpp knob.cpp pffft.cpp ../Core/Src/plinky.c ../Core/Src/gfx.c
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += imgui_impl_glfw.cpp imgui_impl_opengl3.cpp
OBJS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))
DEP = $(OBJS:%.o=%.d)
UNAME_S := $(shell uname -s)
LINUX_GL_LIBS = -lGL

CXXFLAGS = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMGUI_DIR)/gl3w -I$(IMGUI_DIR)/glfw/include -I$(PORTAUDIO_DIR)/include
CXXFLAGS += -Wall -Wformat
LIBS = 

##---------------------------------------------------------------------
## OPENGL LOADER / OPENGL ES
##---------------------------------------------------------------------

## See below for OpenGL ES option (no loader required) - comment out
## the following if you want to use OpenGL ES instead of Desktop GL.

## Using OpenGL loader: gl3w [default]
SOURCES += $(IMGUI_DIR)/gl3w/GL/gl3w.c
CXXFLAGS += -I$(IMGUI_DIR)/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W

## Using OpenGL loader: glew
## (This assumes a system-wide installation)
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GLEW
# LIBS += -lGLEW

## Using OpenGL loader: glad
# SOURCES += ../libs/glad/src/glad.c
# CXXFLAGS += -I../libs/glad/include -DIMGUI_IMPL_OPENGL_LOADER_GLAD

## Using OpenGL loader: glad2
# SOURCES += ../libs/glad/src/gl.c
# CXXFLAGS += -I../libs/glad/include -DIMGUI_IMPL_OPENGL_LOADER_GLAD2

## Using OpenGL loader: glbinding
## This assumes a system-wide installation
## of either version 3.0.0 (or newer)
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GLBINDING3
# LIBS += -lglbinding
## or the older version 2.x
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GLBINDING2
# LIBS += -lglbinding

## Using OpenGL ES, no loader required
## This assumes a GL ES library available in the system, e.g. libGLESv2.so
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_ES2
# LINUX_GL_LIBS = -lGLESv2

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL `pkg-config --static --libs glfw3`

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
	CFLAGS = $(CXXFLAGS)

	# GLFW:
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	# Static:
	LIBS += -L$(IMGUI_DIR)/glfw/lib-macos_x64
	LIBS += -lglfw3
	# Dynamic:
	#LIBS += -lglfw

	# PortAudio:
	LIBS += -L$(PORTAUDIO_DIR)/lib/macos_x64
	LIBS += -lportaudio
	LIBS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices -framework Carbon 
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lglfw3 -lgdi32 -lopengl32 -limm32

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

$(OBJDIR)/%.o:%.cpp
	$(CXX) $(CXXFLAGS) -MMD -std=c++11 -c -o $@ $<

$(OBJDIR)/%.o:../Core/Src/%.c
	$(CC) $(CFLAGS) -MMD -I../Core/Inc -c -o $@ $<

$(OBJDIR)/%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJDIR)/%.o:$(IMGUI_DIR)/gl3w/GL/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

all: executable
	@echo Build complete for $(ECHO_MESSAGE)

debug: CXXFLAGS += -g -DDEBUG
debug: CFLAGS += -g -DDEBUG
debug: executable
	@echo Debug build complete for $(ECHO_MESSAGE)

-include $(DEP)

executable: directories $(EXE)

directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(OBJDIR)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(BASEOBJDIR)/*/*.o $(BASEOBJDIR)/*/*.d $(BASETARGETDIR)/*/$(EXENAME)

