CXX = g++

CXXFLAGS = -std=c++17 -fPIC -Wall -Wextra -O2 -g \
           -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS = -shared -lpthread -ldl -lrt

TARGET = librecar_overlay.so
MANIFEST = recar_layer.json
INSTALL_DIR = $(HOME)/.local/share/vulkan/implicit_layer.d
LIB_INSTALL_DIR = $(HOME)/.local/lib/recar-overlay

SRC_DIR = src

SRCS = $(SRC_DIR)/rvLyMain.cpp \
       $(SRC_DIR)/rvOrRend.cpp \
       $(SRC_DIR)/rvSkServ.cpp \
       $(SRC_DIR)/rvStTrns.cpp \
       $(SRC_DIR)/rvDpDisp.cpp

OBJS = $(SRCS:.cpp=.o)
SHADER_HEADER = $(SRC_DIR)/rvShShdr.hpp

.PHONY: all clean install uninstall shaders

all: shaders $(TARGET)

shaders: $(SHADER_HEADER)

$(SHADER_HEADER): shaders/overlay.vert.glsl shaders/overlay.frag.glsl compile_shaders.py
	python3 compile_shaders.py

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp $(SHADER_HEADER)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

install: all
	@mkdir -p $(LIB_INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(LIB_INSTALL_DIR)/
	sed 's|__LIB_PATH__|$(LIB_INSTALL_DIR)/$(TARGET)|g' recar_layer.json.in > $(INSTALL_DIR)/$(MANIFEST)
	@echo ""
	@echo "library    @ $(LIB_INSTALL_DIR)/$(TARGET)"
	@echo "vkmanifest @ $(INSTALL_DIR)/$(MANIFEST)"
	@echo ""

uninstall:
	rm -f $(INSTALL_DIR)/$(MANIFEST)
	rm -rf $(LIB_INSTALL_DIR)
	rm -f /dev/shm/recar_overlay
	@echo "uninstalled."

clean:
	rm -f $(OBJS) $(TARGET) $(SHADER_HEADER)
	rm -f shaders/*.spv
