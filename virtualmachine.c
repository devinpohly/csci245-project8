#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Architecture definitions */
#define MEMSIZE 500
#define REGS 32
#define IP 0
#define RP 29
#define FP 30
#define SP 31

/* Other constants */
#define MAXLINE 255

/*
 * Prints debugger help message
 */
void help(void)
{
	fprintf(stderr,
"Debugger commands (may be abbreviated):\n"
"  help      - Prints this message\n"
"  quit      - Exits virtual machine completely\n"
"  list [N]  - Disassembles the next N instructions (default 5)\n"
"  print R   - Prints the value of the register named R\n"
"              (also accepts mnemonics IP, RP, FP, SP)\n"
"  x A       - Prints the value in memory at address A\n"
"              (also accepts register names if register holds an address)\n"
"  frame [N] - Displays the top N values on the stack\n"
"              (default from SP to FP)\n"
"  step [N]  - Executes N instructions (default 1)\n"
"  continue  - Runs the program from current state without interruption\n"
"  restart   - Resets the program to initial state\n"
"  break [A] - Creates a `breakpoint' to pause execution whenever IP == A\n"
"              (default is current IP)\n"
"  delete    - Deletes the breakpoint\n"
"\n"
"Commands which accept an address will also accept a register name, in\n"
"which case the value in that register will be used as the address.\n"
"\n"
"Commands which accept a register name also recognize mnemonics for\n"
"special-purpose registers (ip, rp, fp, sp).\n");
}

int read_vmlfile(char *fname, int *memory)
{
	int words, i;
	FILE *source = fopen(fname, "r");
	if (!source) {
		fprintf(stderr, "File not found: `%s'\n", fname);
		return -1;
	}
	fscanf(source, "%d\n", &words);

	if (words < 0) {
		fprintf(stderr, "Bad word count in %s: %d\n", fname, words);
		return -1;
	}

	for(i = 0; i < words; i++) {
		if (fscanf(source, " %d ", memory + i) != 1) {
			if (feof(source)) {
				fprintf(stderr, "File %s advertised %d words but had only %d\n",
						fname, words, i);
			} else {
				fprintf(stderr, "Bad data for word %d in file %s\n", i, fname);
			}
			return -1;
		}
	}

	fclose(source);
	return words;
}

int reset_machine(int *registers, int *memory, char *fname)
{
	int code_size = read_vmlfile(fname, memory);
	if (code_size < 0)
		return 1;

	registers[IP] = 0;
	registers[FP] = registers[SP] = code_size;
	return 0;
}

int get_reg_arg(char *arg)
{
	int regnum;
	if (!strcasecmp(arg, "ip"))
		return IP;
	if (!strcasecmp(arg, "rp"))
		return RP;
	if (!strcasecmp(arg, "fp"))
		return FP;
	if (!strcasecmp(arg, "sp"))
		return SP;
	if (arg[0] != 'r' && arg[0] != 'R')
		return -1;

	regnum = atoi(arg + 1);
	if (regnum < 0 || regnum >= REGS)
		return -1;
	return regnum;
}

int get_addr_arg(char *arg, int *regs)
{
	int regnum, addr;
	if (arg[0] == '\0') {
		fprintf(stderr, "address argument is required\n");
		return -1;
	}

	// Try register name first, then address
	regnum = get_reg_arg(arg);
	if (regnum >= 0) {
		addr = regs[regnum];
	} else if (isdigit(arg[0])) {
		addr = atoi(arg);
	} else {
		fprintf(stderr, "bad address: `%s'\n", arg);
		return -1;
	}

	// Check that address is within memory
	if (addr < 0 || addr >= MEMSIZE) {
		fprintf(stderr, "address %d out of range\n", addr);
		return -1;
	}
	return addr;
}

/*
 * from https://stackoverflow.com/a/4770992/656767
 */
int is_prefix(char *pre, char *str)
{
	return !strncasecmp(pre, str, strlen(pre));
}

/*
 * Executes one instruction.
 *
 * Returns 0 for success, -1 to halt the machine, and 1 if there is an error.
 */
int step(int *registers, int *memory)
{
	int ip = registers[IP];
	int rd, rs, ra, imm;

	switch (memory[ip++]) 
	{
		case 1:  /* MOVI imm rd */
			imm = memory[ip++];
			rd = memory[ip++];
			registers[rd] = imm;
			break;

		case 2:  /* MOV  rs rd */
			rs = memory[ip++];
			rd = memory[ip++];
			registers[rd] = registers[rs];
			break;

		case 3:  /* ADD  rs rd */
			rs = memory[ip++];
			rd = memory[ip++];
			registers[rd] += registers[rs];
			break;

		case 4:  /* SUB  rs rd */
			rs = memory[ip++];
			rd = memory[ip++];
			registers[rd] -= registers[rs];
			break;

		case 5:  /* MUL  rs rd */
			rs = memory[ip++];
			rd = memory[ip++];
			registers[rd] *= registers[rs];
			break;

		case 6:  /* IDIV rs rd */
			rs = memory[ip++];
			rd = memory[ip++];
			registers[rd] /= registers[rs];
			break;

		case 7:  /* JMP  ra */
			ra = memory[ip++];
			ip = registers[ra];
			break;

		case 8:  /* JNZ  rs ra */
			rs = memory[ip++];
			ra = memory[ip++];
			if (registers[rs])
				ip = registers[ra];
			break;

		case 9:  /* OUT  rs */
			rs = memory[ip++];
			printf("%d\n", registers[rs]);
			break;

		case 10: /* HALT */
			return -1;

		case 11: /* LD   ra rd */
			ra = memory[ip++];
			rd = memory[ip++];
			registers[rd] = memory[registers[ra]];
			break;

		case 12: /* ST   ra rs */
			ra = memory[ip++];
			rs = memory[ip++];
			memory[registers[ra]] = registers[rs];
			break;

		case 13: /* JAL  ra */
			ra = memory[ip++];
			registers[RP] = ip;
			ip = registers[ra];
			break;

		case 14: /* RET */
			ip = registers[RP];
			break;

		case 15: /* PUSH rs */
			rs = memory[ip++];
			memory[registers[SP]++] = registers[rs];
			break;

		case 16: /* POP  rd */
			rd = memory[ip++];
			registers[rd] = memory[--registers[SP]];
			break;

		case 17: /* LDLO imm rd */
			imm = memory[ip++];
			rd = memory[ip++];
			registers[rd] = memory[registers[FP] + imm];
			break;

		case 18: /* STLO imm rs */
			imm = memory[ip++];
			rs = memory[ip++];
			memory[registers[FP] + imm] = registers[rs];
			break;

		default:
			fprintf(stderr, "invalid opcode: %d\n", memory[ip - 1]);
			return 1;
	}
	registers[IP] = ip;

	return 0;
}

int read_command(char *cmd, char *arg, int ip) {
	char line[MAXLINE + 1];
	int argc = 0;
	do {
		// Display prompt and read a line
		fprintf(stderr, "dbg[%03d]> ", ip);
		if (!fgets(line, MAXLINE, stdin)) {
			if (ferror(stdin))
				fprintf(stderr, "Unexpected error when reading command\n");
			return -1;
		}
		// Clear buffers and parse command
		cmd[0] = arg[0] = '\0';
		argc = sscanf(line, " %s %s ", cmd, arg);
	} while (argc < 1);

	return argc;
}

int main(int argc, char **argv)
{
	int memory[MEMSIZE];
	int registers[REGS];
	int debug = 0;
	int breakpoint = -1;
	char *fname;
	int filearg = 1;

	if (argc > 1 && !strcmp(argv[1], "-d")) {
		debug = 1;
		filearg++;
	}

	if (filearg >= argc) {
		fprintf(stderr,
			"Missing filename.\n"
			"Usage: %s [-d] program.vml\n", argv[0]);
		return 1;
	}
	fname = argv[filearg];

	if (reset_machine(registers, memory, fname)) {
		return 1;
	}

	int paused = debug;
	for(;;) {
		char cmd[256], arg[256];
		int args;

		if (debug) {
			// Handle breakpoint
			if (!paused && registers[IP] == breakpoint) {
				fprintf(stderr, "Hit breakpoint at %d\n", breakpoint);
				paused = 1;
			}

			if (paused) {
				// Display a prompt, read a command and parse it
				args = read_command(cmd, arg, registers[IP]);
				if (args < 0) {
					fprintf(stderr, "\nEnd of input, continuing program\n");
					debug = 0;
				}
			}
		}

		if (debug && paused && is_prefix(cmd, "continue")) {
			// Continue (make sure one step happens to avoid
			// immediately re-pausing if at a breakpoint)
			paused = 0;
		}

		// Run one step of the machine (default if not in debug mode or
		// if not paused)
		if (!debug || !paused || is_prefix(cmd, "step")) {
			switch (step(registers, memory)) {
				case 0:
					// Normal step
					break;
				case 1:
					// Error
					if (!debug)
						return 1;
					paused = 1;
					break;
				case -1:
					// Halt instruction
					if (debug)
						fprintf(stderr, "Program exited normally\n");
					return 0;
			}
		} else if (is_prefix(cmd, "continue")) {
			// Continue (already handled in a previous if)
		} else if (is_prefix(cmd, "help")) {
			// Help
			help();
		} else if (is_prefix(cmd, "quit")) {
			// Quit
			break;
		} else if (is_prefix(cmd, "list")) {
			// List disassembly
			fprintf(stderr, "list command not yet implemented\n");
		} else if (is_prefix(cmd, "print")) {
			// Print register
			if (args < 2) {
				fprintf(stderr, "print command requires an argument\n");
			} else {
				int regnum = get_reg_arg(arg);
				if (regnum < 0) {
					fprintf(stderr, "bad register name: `%s'\n", arg);
				} else {
					fprintf(stderr, "%s: %d\n", arg, registers[regnum]);
				}
			}
		} else if (!strcmp(cmd, "x")) {
			// Examine memory
			if (args < 2) {
				fprintf(stderr, "x command requires an argument\n");
			} else {
				int addr = get_addr_arg(arg, registers);
				if (addr >= 0)
					fprintf(stderr, "%03d: %d\n", addr, memory[addr]);
			}
		} else if (is_prefix(cmd, "frame")) {
			// Show stack values
			fprintf(stderr, "frame command not yet implemented\n");
		} else if (is_prefix(cmd, "restart")) {
			// Restart
			if (reset_machine(registers, memory, fname)) {
				fprintf(stderr, "Machine reset failed, exiting\n");
				return 1;
			}
		} else if (is_prefix(cmd, "break")) {
			// Set breakpoint
			int addr = registers[IP];
			if (args >= 2)
				addr = get_addr_arg(arg, registers);
			if (addr >= 0) {
				breakpoint = addr;
				fprintf(stderr, "Breakpoint set at address %d\n", breakpoint);
			}
		} else if (is_prefix(cmd, "delete")) {
			// Delete breakpoint
			if (breakpoint < 0) {
				fprintf(stderr, "No breakpoint set\n");
			} else {
				fprintf(stderr, "Breakpoint deleted (was %d)\n", breakpoint);
				breakpoint = -1;
			}
		}
	}
	return 0;
}
