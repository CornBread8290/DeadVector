# Compiler settings
CC = gcc
ASM = ml64
LD = gcc

# Common compiler flags
CFLAGS = -Wall -Wextra -O2 -nostartfiles -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections

# File setup
SRC_C = main.c utils.c    # Add all C source files here
SRC_ASM = asm_code.asm    # Add all MASM assembly files here
OBJ = $(SRC_C:.c=.o) $(SRC_ASM:.asm=.obj)
OUT = mygame.exe          # Change to your desired executable name

# Build executable
all: $(OUT)

$(OUT): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(OUT)

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile MASM Assembly files
%.obj: %.asm
	$(ASM) /c /Fo $@ $<

# Clean build files
clean:
	rm -f $(OBJ) $(OUT)

# Run program
run: $(OUT)
	./$(OUT)
