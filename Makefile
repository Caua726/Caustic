CC = gcc
# -Isrc diz pro compilador procurar headers (.h) dentro da pasta src
CFLAGS = -Wall -Wextra -g -std=c23 -Isrc

# Pastas
SRC_DIR = src
BUILD_DIR = build

# Lista de fontes
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c \
          $(SRC_DIR)/semantic.c $(SRC_DIR)/ir.c $(SRC_DIR)/codegen.c

# Gera a lista de objetos baseada nos fontes, mas trocando o diretório para build/
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET = caustic

# Compila tudo
all: $(TARGET)

# Cria o diretório build se não existir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Linka os objetos para criar o binário final na raiz
$(TARGET): $(BUILD_DIR) $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Compila cada .c da src para .o na build
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza profunda
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.s output main

# Atalho de execução
run: $(TARGET)
	./$(TARGET) main.cst
	# Adicione -nostartfiles entry.s aqui se for usar o runtime assembly
	gcc -no-pie main.cst.s -o main
	./main

# Assembler
assembler: $(TARGET)
	./$(TARGET) caustic-assembler/main.cst
	gcc -no-pie caustic-assembler/main.cst.s -o caustic-as