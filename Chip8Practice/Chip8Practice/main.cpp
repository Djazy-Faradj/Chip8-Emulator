#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <random>

constexpr uint16_t START_ADDRESS = 0x200; // Starting address for Chip8 programs
constexpr unsigned int FONTSET_SIZE = 80; // The font set size
// The bytes representing each character in binary
constexpr uint8_t fontset[FONTSET_SIZE] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// Compute power
int power(int x, int y) {
	int result = 1;
	for (int i = 0; i < y; i++) {
		result *= x;
	}
	return result;
}

class Chip8 {
public:
	Chip8();
	uint8_t registers[16]{};	// 16 8-bit registers (2^4)
	
	uint8_t memory[4096]{};		// 4K of memory (2^12)
	uint16_t index{};			// 16-bit index register
	uint16_t pc{};				// 16-bit program counter

	uint16_t stack[16]{};		// 16 levels of stack (2^4)
	uint8_t sp{};				// Stack pointer

	uint8_t delay_timer{};		// 8-bit delay timer
	uint8_t sound_timer{};		// 8-bit sound timer

	uint8_t keypad[16]{};		// 16 keys (2^4)

	uint32_t screen[64 * 32]{}; // 64x32 pixel screen (2^6 * 2^5)
	uint16_t opcode{};			// Current opcode

	uint8_t genRand() {
		// 1) Create a random-device to seed our engine (non-deterministic)
		std::random_device rd;
		// 2) Choose a pseudo-random number engine
		std::mt19937 gen(rd());
		// 3) Define a distribution on the range you want, e.g., [0, 9]
		std::uniform_int_distribution<> dist(0, 255);

		// Generate some numbers 
		uint8_t n = dist(gen);
		return n;
	}

	// ************ INSTRUCTIONS ************
	// CLS - 00E0 - Clear the display
	void OP_00E0() {
		unsigned int screenByteSize = sizeof(screen) / 4;
		for (int i = 0; i < screenByteSize; i++) {
			screen[i] = 0x00000000;
		}
	}

	// RET - 00EE - Return from a subroutine
	void OP_00EE() {
		pc = stack[--sp];
	}
	
	// JP addr - Jump to location nnn
	void OP_1nnn() {
		uint16_t address = opcode & 0x0FFFu;
		pc = address;
	}

	// CALL addr - 2nnn - Call subroutine at nnn
	void OP_2nnn() {
		stack[sp++] = pc;
		uint16_t jmpAddress = opcode & 0x0FFFu;
		pc = jmpAddress;
	}

	// SE Vx, byte - 3xkk - Skip next instruction if Vx = kk
	void OP_3xkk() {
		uint8_t regIndex = (opcode & 0x0F00) / power(2, 8); // Which register to compare
		uint8_t valueToCompare = (opcode & 0x00FF);

		if (registers[regIndex] == valueToCompare) { // Values are equal, increment pc by 2
			pc += 2;
		}
	}
	// **************************************


private:
	void loadFonts() { // Loads the font set in the chip's memory
		int pos = 0x50; // Font set start address
		for (int i = 0; i < FONTSET_SIZE; i++) {
			memory[pos + i] = fontset[i];
		}
	}
};

Chip8::Chip8() { // Constructor of the Chip
	// Initialize PC 
	pc = 0x200;
	// Load the fonts into memory
	loadFonts();
}

// Function that loads ROM content into memory
void loadROM(const char* filename, Chip8& chip8) {
	FILE* file = nullptr;
	errno_t err = fopen_s(&file, filename, "rb");

	// Check if there are issues with the ROM
		// Failed to open ROM
	if (err != 0 || file == nullptr) {
		std::cerr << "Failed to open ROM file: " << filename << std::endl;
		exit(-1);
	}
	// ROM size too big
	fseek(file, 0, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::cout << "File size: " << fileSize << std::endl;
	if (fileSize > (sizeof(chip8.memory) - START_ADDRESS)) {
		std::cerr << "Size of ROM file is too big: " << filename << std::endl;
		exit(-1);
	}

	// Read the ROM content into memory starting at address 0x200
	uint16_t pos = START_ADDRESS;
	while (fread(&chip8.memory[pos++], 1, 1, file)) {}
	fclose(file);
}

int main() {
	std::cout << "Chip8 Emulator!" << std::endl;
	Chip8* chip8 = new Chip8();
	chip8->OP_3xkk();
	return 0;
}
