//
// simple encoding examples for IA-32 validtion project
//

#include <stdio.h>
#include <stdlib.h>
#include "ia32_encode.h"

// globals to aid debug to start
volatile char *mptr=0,*next_ptr=0;
int num_inst=0;

main(int argc, char *argv[])
{
	int ibuilt=0;
	int num_instructions = 26; // default to 12 instructions

	fprintf(stderr, "Generating %d instructions...\n", num_instructions);

	/* allocate buffer to build instructions into */
	mptr=malloc(MAX_INSTR_BYTES);
	next_ptr=mptr;                  // init next_ptr

	ibuilt=build_instructions(num_instructions);  	// build instructions

	
	fprintf(stderr,"generation program complete, instructions generated: %d\n",ibuilt);
}

//
// Routine:  build_instructions
//
// Description: Generate specified number of instructions using different types
//
// INPUT: num_instructions - number of instructions to generate
// 
// OUTPUT: return the number of instructions built
// 

int build_instructions(int num_instructions) {
	num_inst = 0; // reset counter
	fprintf(stderr, "Building %d instructions (XCHG/XADD/Fence testing)...\n", num_instructions);

	/*********************************************************************************
	 XCHG Register-to-Register Instructions (Testing different sizes and registers)
	 *********************************************************************************/

	// XCHG reg-to-reg: 8-bit (ISZ_1)
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_1, REG_RAX, REG_RBX, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%bl,%%al (8-bit reg-to-reg) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-reg: 16-bit (ISZ_2)
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_2, REG_RCX, REG_RDX, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%dx,%%cx (16-bit reg-to-reg) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-reg: 32-bit (ISZ_4)
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_RSI, REG_RDI, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%edi,%%esi (32-bit reg-to-reg) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-reg: Extended registers (32-bit)
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_R8, REG_R12, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%r12d,%%r8d (32-bit extended regs) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 XCHG Register-to-Memory Instructions (Testing different displacements)
	 *********************************************************************************/

	// XCHG reg-to-memory: 8-bit, no displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_1, REG_RSI, REG_RAX, 0, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%al,(%%rsi) (8-bit mem, no disp) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-memory: 16-bit, 8-bit displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_2, REG_RBX, REG_RCX, 8, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%cx,0x8(%%rbx) (16-bit mem, 8-bit disp) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-memory: 32-bit, 32-bit displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_RDX, REG_RDI, 0x200, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%edi,0x200(%%rdx) (32-bit mem, 32-bit disp) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XCHG reg-to-memory: Extended registers
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_R10, REG_R15, 16, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XCHG %%r15d,0x10(%%r10) (32-bit extended mem) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 LOCK XCHG Instructions (Atomic operations)
	 *********************************************************************************/

	// LOCK XCHG: 8-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_1, REG_RSI, REG_RAX, 0, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XCHG %%al,(%%rsi) (atomic 8-bit) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XCHG: 16-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_2, REG_RBX, REG_RCX, 4, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XCHG %%cx,0x4(%%rbx) (atomic 16-bit) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XCHG: 32-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_RDX, REG_RDI, 12, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XCHG %%edi,0xc(%%rdx) (atomic 32-bit) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XCHG: Extended registers
	if (num_inst < num_instructions) {
		next_ptr = build_xchg(ISZ_4, REG_R11, REG_R14, 0x100, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XCHG %%r14d,0x100(%%r11) (atomic extended) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 XADD Register-to-Register Instructions (Exchange and Add)
	 *********************************************************************************/

	// XADD reg-to-reg: 8-bit
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_1, REG_RAX, REG_RBX, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%bl,%%al (8-bit exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XADD reg-to-reg: 16-bit
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_2, REG_RCX, REG_RDX, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%dx,%%cx (16-bit exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XADD reg-to-reg: 32-bit
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_4, REG_RSI, REG_RDI, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%edi,%%esi (32-bit exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XADD reg-to-reg: Extended registers
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_4, REG_R9, REG_R13, -1, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%r13d,%%r9d (32-bit extended exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 XADD Register-to-Memory Instructions
	 *********************************************************************************/

	// XADD reg-to-memory: 8-bit, no displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_1, REG_RSI, REG_RAX, 0, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%al,(%%rsi) (8-bit mem exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XADD reg-to-memory: 16-bit, 8-bit displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_2, REG_RBX, REG_RCX, 16, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%cx,0x10(%%rbx) (16-bit mem exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// XADD reg-to-memory: 32-bit, 32-bit displacement
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_4, REG_RDX, REG_RDI, 0x300, 0, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: XADD %%edi,0x300(%%rdx) (32-bit mem exchange-add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 LOCK XADD Instructions (Atomic Exchange and Add)
	 *********************************************************************************/

	// LOCK XADD: 8-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_1, REG_RSI, REG_RAX, 8, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XADD %%al,0x8(%%rsi) (atomic 8-bit add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XADD: 16-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_2, REG_RBX, REG_RCX, 20, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XADD %%cx,0x14(%%rbx) (atomic 16-bit add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XADD: 32-bit memory
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_4, REG_RDX, REG_RDI, 24, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XADD %%edi,0x18(%%rdx) (atomic 32-bit add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LOCK XADD: Extended registers
	if (num_inst < num_instructions) {
		next_ptr = build_xadd(ISZ_4, REG_R11, REG_R8, 0x400, 1, next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LOCK XADD %%r8d,0x400(%%r11) (atomic extended add) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	/*********************************************************************************
	 Memory Fence Instructions (Memory ordering)
	 *********************************************************************************/

	// MFENCE - Full memory barrier
	if (num_inst < num_instructions) {
		next_ptr = build_mfence(next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: MFENCE (full memory barrier) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// SFENCE - Store memory barrier
	if (num_inst < num_instructions) {
		next_ptr = build_sfence(next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: SFENCE (store memory barrier) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	// LFENCE - Load memory barrier
	if (num_inst < num_instructions) {
		next_ptr = build_lfence(next_ptr);
		num_inst++;
		fprintf(stderr, "Inst %d: LFENCE (load memory barrier) - next ptr: 0x%lx\n", num_inst, (long)next_ptr);
	}

	fprintf(stderr, "Instruction generation complete! Focus: XCHG/XADD/Fence instructions\n");
	return (num_inst);
}




