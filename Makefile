CC = gcc
CFLAGS = -Wall -Wextra -g -std=c23
SOURCES = main.c lexer.c parser.c semantic.c ir.c codegen.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = caustic

# Compila tudo
all: $(TARGET)

# Linka os objetos
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Compila cada .c para .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpa binários e arquivos temporários (.s, .o)
clean:
	rm -f $(OBJECTS) $(TARGET) *.s *.o

# Um atalho para compilar e rodar um teste rápido
# Uso: make run FILE=main.cst
run: $(TARGET)
	./$(TARGET) $(FILE)
	gcc -no-pie $(FILE).s -o output
	./output