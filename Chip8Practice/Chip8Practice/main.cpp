#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <string>
#include <random>
#include <chrono>

constexpr uint16_t START_ADDRESS = 0x200; // Starting address for Chip8 programs
constexpr unsigned int FONTSET_SIZE = 80; // The font set size
constexpr unsigned int FONTSET_START_ADDRESS = 0x50; // Start address for writing font data
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

constexpr uint8_t SCREEN_WIDTH = 64;
constexpr uint8_t SCREEN_HEIGHT = 32;

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

	// SE Vx, byte - 3xkk - Skip next instruction if Vx == kk
	void OP_3xkk() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u; // Which register to compare
		uint8_t valueToCompare = (opcode & 0x00FFu);

		if (registers[regX] == valueToCompare) { // Values are equal, increment pc by 2
			pc += 2;
		}
	}

	// SNE Vx, byte - 4xkk - Skip next instruction if Vx != kk
	void OP_4xkk() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u; // Which register to compare
		uint8_t valueToCompare = (opcode & 0x00FFu);

		if (registers[regX] != valueToCompare) // Values are not equal, increment pc by 2
			pc += 2;
	}

	// SE Vx, Vy - 5xy0 - Skip next instruction if Vx == Vy
	void OP_5xy0() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u; // Index of register X
		uint8_t regY = (opcode & 0x00F0u) >> 4u; // Index of register Y

		if (registers[regX] == registers[regY])
			pc += 2;
	}

	// LD Vx, byte - 6xkk - The interpreter puts the value kk into register Vx
	void OP_6xkk() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t value = opcode & 0x00FFu;
		registers[regX] = value;
	}

	// ADD Vx, byte - 7xkk - Adds the value kk to the value of register Vx, then stores the result in Vx
	void OP_7xkk() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t value = opcode & 0x00FFu;
		registers[regX] += value;
	}

	// LD Vx, Vy - 8xy0 - Stores the value of register Vy in register Vx
	void OP_8xy0() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		registers[regX] = registers[regY];
	}

	// OR Vx, Vy - 8xy1 - Performs a bitwise OR on the values of Vx and Vy, stored in Vx
	void OP_8xy1() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		registers[regX] = registers[regX] | registers[regY];
	}

	// AND Vx, Vy - 8xy2 - Performs a bitwise AND on the values of Vx and Vy, stored in Vx
	void OP_8xy2() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		registers[regX] = registers[regX] & registers[regY];
	}
	
	// XOR Vx, Vy - 8xy3 - Performs a bitwise XOR on the values of Vx and Vy, stored in Vx
	void OP_8xy3() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		registers[regX] = registers[regX] ^ registers[regY];
	}

	// ADD Vx, Vy | 8xy4 | Performs a bitwise AND on the values of Vx and Vy, stored in Vx
	void OP_8xy4() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		uint16_t result = registers[regX] + registers[regY]; // So we can check for carry
		if (result > 255u) registers[0xF] = 1;	// Set the carry to 1
		else registers[0xF] = 0;				// No carry, set it to 0
		registers[regX] = result & 0x00FFu;
	}

	// SUB Vx, Vy | 8xy5 | Set Vx = Vx - Vy, set VF = NOT borrow
	void OP_8xy5() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		if (registers[regX] > registers[regY]) registers[0xF] = 1;
		else registers[0xF] = 0;
		registers[regX] = registers[regX] - registers[regY];
	}

	// SHR Vx {, Vy} | 8xy6 | Set Vx = Vx SHR 1
	void OP_8xy6() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		registers[0xF] = registers[regX] & 0x1;
		registers[regX] >>= 1;
	}

	// SUBN Vx, Vy | 8xy7 | Set Vx = Vy - Vx, set VF = NOT borrow
	void OP_8xy7() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		if (registers[regX] > registers[regY]) registers[0xF] = 1;
		else registers[0xF] = 0;
		registers[regX] = registers[regX] - registers[regY];
	}

	// SHL Vx {, Vy} | 8xyE | Set Vx = Vx 
	void OP_8xyE() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		registers[0xF] = registers[regX] & 0x1;
		registers[regX] <<= 1;
	}

	// SNE Vx, Vy | 9xy0 | Skip next instruction if Vx != Vy
	void OP_9xy0() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		if (registers[regX] != registers[regY])
			pc += 2;
	}

	// LD I, addr | Annn | The value of register I is set to nnn
	void OP_Annn() {
		uint16_t value = opcode & 0x0FFFu;
		index = value;
	}

	// JP V0, addr | Bnnn | Jump to location nnn + V0
	void OP_Bnnn() {
		uint16_t value = opcode & 0x0FFFu;
		pc = value + registers[0x0];
	}

	// RND Vx, byte | Cxkk | Set Vx = random byte AND kk
	void OP_Cxkk() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t randVal = genRand();
		uint8_t value = opcode & 0x00FF;
		registers[regX] = randVal & value;
	}

	// DRW Vx, Vy, nibble | Dxyn | Display n-byte sprite starting at memory location I at (Vx, Vy),  set VF = collision
	void OP_Dxyn() {
		uint16_t startAddress = index;
		uint8_t byteCount = opcode & 0x000Fu;
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t regY = (opcode & 0x00F0u) >> 4u;
		uint8_t xCoord = registers[regX];
		uint8_t yCoord = registers[regY];

		registers[0xF] = 0;

		for (unsigned int row = 0; row < byteCount; ++row)
		{
			uint8_t spriteByte = memory[index + row];

			for (unsigned int col = 0; col < 8; ++col)
			{
				uint8_t spritePixel = spriteByte & (0x80u >> col);
				uint32_t* screenPixel = &screen[(yCoord + row) * SCREEN_WIDTH + (xCoord + col)];
				// Sprite pixel is on
				if (spritePixel) {
					// Screen pixel also on - collision
					if (*screenPixel == 0xFFFFFFFF) {
						registers[0xF] = 1;
					}
					// Effectively XOR with the sprite pixel
					*screenPixel ^= 0xFFFFFFFF;
				}
			}
		}
	}

	// SKP Vx | Ex9E | Skip next instruction if key with the value of Vx is pressed
	void OP_Ex9E() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t key = registers[regX];
		if (keypad[key]) pc += 2;
	}
	
	// SKNP Vx | ExA1 | Skip next instruction if key with the value of Vx is not pressed
	void OP_ExA1() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t key = registers[regX];
		if (!keypad[key]) pc += 2;
	}

	// LD Vx, DT | Fx07 | The value of DT is placed into Vx
	void OP_Fx07() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		registers[regX] = delay_timer;
	}

	// Potential mistake**LD Vx, K | Fx0A | Wait for a key press, store the value of the key in Vx
	void OP_Fx0A() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		while (true) {
			for (int i = 0; i < 16; i++) {
				if (keypad[i]) {
					registers[regX] = i;
					return;
				}
			}
		}
	}

	// LD DT, Vx | Fx15 | Set delay timer = Vx
	void OP_Fx15() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		delay_timer = registers[regX];
	}

	// LD ST, Vx | Fx18 | Set sound timer = Vx
	void OP_Fx18() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		sound_timer = registers[regX];
	}

	// ADD I, Vx | Fx1E | The values of I and Vx are added, and the results are stored in I
	void OP_Fx1E() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		index += registers[regX];
	}

	// LD F, Vx | Fx29 | Set I = location of sprite for digit Vx
	void OP_Fx29() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t value = registers[regX];
		index = FONTSET_START_ADDRESS + (registers[regX] + 5);
	}

	// LD B, Vx | Fx33 | Store BCD representation of Vx in memory locations I, I+1, and I+2
	void OP_Fx33() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		uint8_t value = registers[regX];
		uint8_t hundreds = value / 100;
		uint8_t tens = (value - (hundreds * 100)) / 10;
		uint8_t ones = (value - (hundreds * 100) - (tens * 10));
		memory[index] = hundreds;
		memory[index + 1] = tens;
		memory[index + 2] = ones;
	}

	// LD [I], Vx | Fx55 | Store registers V0 through Vx in memory starting at location I
	void OP_Fx55() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		for (uint8_t i = 0; i <= regX; i++) {
			memory[index + i] = registers[i];
		}
	}

	// LD Vx, [I] | Fx65 | Read registers V0 through Vx from memory starting at location I
	void OP_Fx65() {
		uint8_t regX = (opcode & 0x0F00u) >> 8u;
		for (uint8_t i = 0; i <= regX; i++) {
			registers[i] = memory[index + i];
		}
	}
	
	void OP_NULL(){}
	// **************************************

	void (Chip8::*table[16])(){};
	void (Chip8::* table0[15])() {};
	void (Chip8::* table8[15])() {};
	void (Chip8::* tableE[15])() {};
	void (Chip8::* tableF[102])() {};

	void Table0() {
		((*this).*(table0[opcode & 0x000Fu]))();
	}

	void Table8() {
		((*this).*(table8[opcode & 0x000Fu]))();
	}

	void TableE() {
		((*this).*(tableE[opcode & 0x000Fu]))();
	}

	void TableF() {
		((*this).*(tableF[opcode & 0x00FFu]))();
	}

	void Cycle() {
		// Fetch
		opcode = (memory[pc] << 8u) + memory[pc + 1];
		pc += 2;

		// Decode and Execute
		((*this).*(table[(opcode & 0xF000u) >> 12u]))();

		// Update timers for delay and sound
		if (delay_timer > 0) --delay_timer;
		if (sound_timer > 0) --sound_timer;
	}
private:
	void loadFonts() { // Loads the font set in the chip's memory
		int pos = FONTSET_START_ADDRESS; // Font set start address
		for (int i = 0; i < FONTSET_SIZE; i++) {
			memory[pos + i] = fontset[i];
		}
	}

	void initTables() {
		// Start by filling the sub tables with OP_NULL
		for (int i = 0; i < 102; i++) {
			if (i < 16) {
				table0[i] = &Chip8::OP_NULL;
				table8[i] = &Chip8::OP_NULL;
				tableE[i] = &Chip8::OP_NULL;
			}
			tableF[i] = &Chip8::OP_NULL;
		}

		// Set up function pointer tables
		table[0x0] = &Chip8::Table0;
		table[0x1] = &Chip8::OP_1nnn;
		table[0x2] = &Chip8::OP_2nnn;
		table[0x3] = &Chip8::OP_3xkk;
		table[0x4] = &Chip8::OP_4xkk;
		table[0x5] = &Chip8::OP_5xy0;
		table[0x6] = &Chip8::OP_6xkk;
		table[0x7] = &Chip8::OP_7xkk;
		table[0x8] = &Chip8::Table8;
		table[0x9] = &Chip8::OP_9xy0;
		table[0xA] = &Chip8::OP_Annn;
		table[0xB] = &Chip8::OP_Bnnn;
		table[0xC] = &Chip8::OP_Cxkk;
		table[0xD] = &Chip8::OP_Dxyn;
		table[0xE] = &Chip8::TableE;
		table[0xF] = &Chip8::TableF;

		// table0
		table0[0x0] = &Chip8::OP_00E0;
		table0[0xE] = &Chip8::OP_00EE;

		// tableE
		tableE[0x1] = &Chip8::OP_ExA1;
		tableE[0xE] = &Chip8::OP_Ex9E;

		// table8
		table8[0x0] = &Chip8::OP_8xy0;
		table8[0x1] = &Chip8::OP_8xy1;
		table8[0x2] = &Chip8::OP_8xy2;
		table8[0x3] = &Chip8::OP_8xy3;
		table8[0x4] = &Chip8::OP_8xy4;
		table8[0x5] = &Chip8::OP_8xy5;
		table8[0x6] = &Chip8::OP_8xy6;
		table8[0x7] = &Chip8::OP_8xy7;
		table8[0xE] = &Chip8::OP_8xyE;

		// tableF
		tableF[0x07] = &Chip8::OP_Fx07;
		tableF[0x0A] = &Chip8::OP_Fx0A;
		tableF[0x15] = &Chip8::OP_Fx15;
		tableF[0x18] = &Chip8::OP_Fx18;
		tableF[0x1E] = &Chip8::OP_Fx1E;
		tableF[0x29] = &Chip8::OP_Fx29;
		tableF[0x33] = &Chip8::OP_Fx33;
		tableF[0x55] = &Chip8::OP_Fx55;
		tableF[0x65] = &Chip8::OP_Fx65;
	}
};

Chip8::Chip8() { // Constructor of the Chip
	// Initialize PC 
	pc = 0x200;
	// Load the fonts into memory
	loadFonts();
	// Setup the tables
	initTables();
}

class Platform {
private:
	SDL_Window* window{};
	SDL_Renderer* renderer{};
	SDL_Texture* texture{};
public:
	Platform(char* windowTitle, int windowWidth, int windowHeight, int textureWidth, int textureHeight) {
		SDL_Init(SDL_INIT_VIDEO);
		window = SDL_CreateWindow(windowTitle, windowWidth, windowHeight, NULL);
		renderer = SDL_CreateRenderer(window, NULL);
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, textureWidth, textureHeight);
		
		// Set the texture scale to nearest
		SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
	}

	~Platform() {
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	void Update(void const* buffer, int pitch) {
		SDL_UpdateTexture(texture, nullptr, buffer, pitch);
		SDL_RenderClear(renderer);
		SDL_RenderTexture(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
	}

	bool ProcessInput(uint8_t* keys) {
		bool quit = false;

		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
			{
				quit = true;
			} break;

			case SDL_EVENT_KEY_DOWN:
			{
				switch (event.key.key)
				{
				case SDLK_ESCAPE:
				{
					quit = true;
				} break;

				case SDLK_X:
				{
					keys[0] = 1;
				} break;

				case SDLK_1:
				{
					keys[1] = 1;
				} break;

				case SDLK_2:
				{
					keys[2] = 1;
				} break;

				case SDLK_3:
				{
					keys[3] = 1;
				} break;

				case SDLK_Q:
				{
					keys[4] = 1;
				} break;

				case SDLK_W:
				{
					keys[5] = 1;
				} break;

				case SDLK_E:
				{
					keys[6] = 1;
				} break;

				case SDLK_A:
				{
					keys[7] = 1;
				} break;

				case SDLK_S:
				{
					keys[8] = 1;
				} break;

				case SDLK_D:
				{
					keys[9] = 1;
				} break;

				case SDLK_Z:
				{
					keys[0xA] = 1;
				} break;

				case SDLK_C:
				{
					keys[0xB] = 1;
				} break;

				case SDLK_4:
				{
					keys[0xC] = 1;
				} break;

				case SDLK_R:
				{
					keys[0xD] = 1;
				} break;

				case SDLK_F:
				{
					keys[0xE] = 1;
				} break;

				case SDLK_V:
				{
					keys[0xF] = 1;
				} break;
				}
			} break;

			case SDL_EVENT_KEY_UP:
			{
				switch (event.key.key)
				{
				case SDLK_X:
				{
					keys[0] = 0;
				} break;

				case SDLK_1:
				{
					keys[1] = 0;
				} break;

				case SDLK_2:
				{
					keys[2] = 0;
				} break;

				case SDLK_3:
				{
					keys[3] = 0;
				} break;

				case SDLK_Q:
				{
					keys[4] = 0;
				} break;

				case SDLK_W:
				{
					keys[5] = 0;
				} break;

				case SDLK_E:
				{
					keys[6] = 0;
				} break;

				case SDLK_A:
				{
					keys[7] = 0;
				} break;

				case SDLK_S:
				{
					keys[8] = 0;
				} break;

				case SDLK_D:
				{
					keys[9] = 0;
				} break;

				case SDLK_Z:
				{
					keys[0xA] = 0;
				} break;

				case SDLK_C:
				{
					keys[0xB] = 0;
				} break;

				case SDLK_4:
				{
					keys[0xC] = 0;
				} break;

				case SDLK_R:
				{
					keys[0xD] = 0;
				} break;

				case SDLK_F:
				{
					keys[0xE] = 0;
				} break;

				case SDLK_V:
				{
					keys[0xF] = 0;
				} break;
				}
			} break;
			}
		}

		return quit;
	}
};

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

int main(int argc, char* argv[]) {
	std::cout << "Chip8 Emulator -- Djazy Faradj" << std::endl;
	
	if (argc != 4) {
		std::cerr << "Usage: " << argv[0] << " <Scale> <Delay> <ROM>\n";
		int i;
		std::cout << "Press Q + ENTER to close.";
		std::cin >> i;
		return -1;
	}

	int scale = std::stoi(argv[1]);
	int cycleDelay = std::stoi(argv[2]);
	char* romFilename = argv[3];

	Platform* platform = new Platform((char*)"Chip-8 Emulator", SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SCREEN_WIDTH, SCREEN_HEIGHT); // Start SDL Platform
	Chip8* chip8 = new Chip8(); // Instanciate chip

	loadROM(romFilename, *chip8); // Load ROM in the chip
	int videoPitch = sizeof(chip8->screen[0]) * SCREEN_WIDTH;

	auto lastCycleTime = std::chrono::high_resolution_clock::now();
	bool quit = false;

	while (!quit) {
		quit = platform->ProcessInput(chip8->keypad);
		auto currentTime = std::chrono::high_resolution_clock::now();

		float dt = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastCycleTime).count();
		if (dt > cycleDelay) {
			lastCycleTime = currentTime;
			chip8->Cycle();
			platform->Update(chip8->screen, videoPitch);
		}
	}
	return 0;
}
