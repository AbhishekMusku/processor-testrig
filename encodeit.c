//
// simple encoding example fr IA-32 validation project
//
// project_part2
// 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "ia32_encode.h"


#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

// globals to aid debug to start
volatile char *mptr=0,*next_ptr=0, *mdptr=0;
int num_inst=25;
unsigned seed = 12345;

// declarations for starting test
typedef int (*funct_t)();
funct_t start_test;
int executeit();

main(int argc, char *argv[])
{

	int ibuilt=0,rc=0;
	
    // Parse command line arguments
    if (argc >= 2) {
        seed = atoi(argv[1]);
    }
    if (argc >= 3) {
        num_inst = atoi(argv[2]);
    }
    

	/* allocate buffer to perform stores and loads to, and set permissions  */

	mdptr = (volatile char *)mmap(
		(void *) 0,
		(MAX_DATA_BYTE+PAGESIZE-1),
		PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_SHARED,
		0, 0
		);

	if(mdptr == MAP_FAILED) { 
		printf("data mptr allocation failed\n"); 
		exit(1); 
	}


	/* allocate buffer to build instructions into, and set permissions to allow execution of this memory area */

	mptr = (volatile char *)mmap(
		(void *) 0,
		(MAX_INSTR_BYTES+PAGESIZE-1),
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_SHARED,
		0, 0
		);

	if(mptr == MAP_FAILED) { 
		printf("instr  mptr allocation failed\n"); 
		exit(1); 
	}

	next_ptr=mptr;                  // init next_ptr

	ibuilt=build_instructions();  	// build instructions

	/* ok now that I built the critters, time to execute them */

	start_test=(funct_t) mptr;
	executeit(start_test);


	fprintf(stderr,"generation program complete, %d instructions generated, and executed\n",ibuilt);

	// clean up the allocation before getting out

	munmap((caddr_t)mdptr,(MAX_DATA_BYTE+PAGESIZE-1));
	munmap((caddr_t)mptr,(MAX_INSTR_BYTES+PAGESIZE-1));


}

/*
 * Function: executeit
 * 
 * Description:
 *
 * This function will start executing at the function address passed into it 
 * and return an integer return value that will be used to indicate pass(0)/fail(1)
 *
 * INTPUTs:  funct_t start_addr :      function pointer 
 *
 * Returns:  int                :      0 for pass, 1 for fail
 */   
int executeit(funct_t start_addr) 
{

	volatile int i,rc=0;

	i=0;

	rc=(*start_addr)();

	return(0);
}

static inline volatile char *enter(short stack_size, char nesting_level, volatile char *tgt_addr)
{
    // ENTER instruction opcode
    *tgt_addr = 0xC8;
    tgt_addr += BYTE1_OFF;
    
    // 16-bit stack size (little-endian format)
    *(short *)tgt_addr = stack_size;
    tgt_addr += BYTE2_OFF;
    
    // 8-bit nesting level  
    *tgt_addr = nesting_level;
    tgt_addr += BYTE1_OFF;
    
    return tgt_addr;
}

static inline volatile char *leave(volatile char *tgt_addr)
{
    // LEAVE instruction: C9
    *tgt_addr = 0xC9;
    tgt_addr += BYTE1_OFF;
    
    return tgt_addr;
}

static inline volatile char *ret(volatile char *tgt_addr)
{
    // RET instruction: C3
    *tgt_addr = 0xC3;
    tgt_addr += BYTE1_OFF;
    
    return tgt_addr;
}

static inline volatile char *add_headeri(volatile char *tgt_addr)
{
	// Create stack frame with ENTER $2048, 0
    tgt_addr = enter(2048, 0, tgt_addr);
	
    // Save callee-saved registers (x86-64 ABI)
    // Push RBX (no REX needed)
    tgt_addr = build_push_reg(REG_RBX, 0, tgt_addr);
    
    // Push R12 (needs REX.B)
    tgt_addr = build_push_reg(REG_R12, 1, tgt_addr);
    
    // Push R13 (needs REX.B)
    tgt_addr = build_push_reg(REG_R13, 1, tgt_addr);
    
    // Push R14 (needs REX.B)
    tgt_addr = build_push_reg(REG_R14, 1, tgt_addr);
    
    // Push R15 (needs REX.B)
    tgt_addr = build_push_reg(REG_R15, 1, tgt_addr);
    
    return tgt_addr;
}

static inline volatile char *add_endi(volatile char *tgt_addr)
{    
    // Restore callee-saved registers in REVERSE order (LIFO stack)
    // Order: R15, R14, R13, R12, RBX
    
    // Pop R15 (needs REX.B)
    tgt_addr = build_pop_reg(REG_R15, 1, tgt_addr);
    
    // Pop R14 (needs REX.B)
    tgt_addr = build_pop_reg(REG_R14, 1, tgt_addr);
    
    // Pop R13 (needs REX.B)
    tgt_addr = build_pop_reg(REG_R13, 1, tgt_addr);
    
    // Pop R12 (needs REX.B)
    tgt_addr = build_pop_reg(REG_R12, 1, tgt_addr);
    
    // Pop RBX (no REX needed)
    tgt_addr = build_pop_reg(REG_RBX, 0, tgt_addr);
	
	// Release stack frame
    tgt_addr = leave(tgt_addr);
    
    // Return to caller
    tgt_addr = ret(tgt_addr);
    
    return tgt_addr;
}

//
// Routine:  build_instructions
//
// Description:
//
// INPUT: none yet
// 
// OUTPUT: returns the number of instructions built
// 
int build_instructions() {
    // Initialize randomization
    srand(seed);
    
    // Available registers (excluding RBP=5, RSP=4, RSI=6, R12=12, R13=13)
    int safe_registers[] = {0, 1, 2, 3, 7, 8, 9, 10, 11, 14, 15};
    int num_safe_regs = 11;
    
    // Instruction types to randomize among
    enum instr_type {
        INSTR_REG_TO_REG = 0,
        INSTR_IMM_TO_REG = 1, 
        INSTR_REG_TO_MEM = 2,
        INSTR_MEM_TO_REG = 3,
        INSTR_XADD_REG = 4,      // XADD reg-to-reg
        INSTR_XADD_MEM = 5,      // XADD reg-to-memory  
        INSTR_XCHG_REG = 6,      // XCHG reg-to-reg
        INSTR_XCHG_MEM = 7       // XCHG reg-to-memory
    };
    int num_instr_types = 8;
    
    // Displacement types for memory operations
    enum disp_type {
        DISP_0 = 0,
        DISP_8 = 1,     
        DISP_32 = 2
    };
    int num_disp_types = 3;
    
    // Valid instruction sizes
    int valid_sizes[] = {ISZ_1, ISZ_4, ISZ_8};
    int valid_sizes_all[] = {ISZ_1, ISZ_2, ISZ_4, ISZ_8};
    
    // Calling the header
    next_ptr = add_headeri(next_ptr);
    
    // Set up RSI with mdptr for memory operations
    next_ptr = build_imm_to_register(ISZ_8, (long)mdptr, REG_RSI, next_ptr);
    fprintf(stderr, "MOVING MDPTR: MOV #%lX->R%d (size=%d)\n", (long)mdptr, REG_RSI, ISZ_8);
    num_inst++;
    fprintf(stderr, "Setup: loaded mdptr into RSI\n");
    
    int i;
    // Generate random instructions
    for (i = 0; i < num_inst; i++) {
        // Pick random instruction type
        int instr_type = rand() % num_instr_types;
        
        // Pick random registers
        int reg1 = safe_registers[rand() % num_safe_regs];
        int reg2 = safe_registers[rand() % num_safe_regs];
        
        // Ensure reg1 != reg2 for reg-to-reg operations
        while (reg1 == reg2 && (instr_type == INSTR_REG_TO_REG || 
                               instr_type == INSTR_XADD_REG || 
                               instr_type == INSTR_XCHG_REG)) {
            reg2 = safe_registers[rand() % num_safe_regs];
        }
        
		// Pick random size - FIXED VERSION
		int size;

		// Special handling for XADD/XCHG (no ISZ_8 support)
		if (instr_type >= INSTR_XADD_REG && instr_type <= INSTR_XCHG_MEM) {
			if (reg1 >= 8 || reg2 >= 8) {
				int xadd_sizes[] = {ISZ_1, ISZ_4};  // No ISZ_2, ISZ_8
				size = xadd_sizes[rand() % 2];
			} else {
				int xadd_sizes_all[] = {ISZ_1, ISZ_2, ISZ_4};  // No ISZ_8
				size = xadd_sizes_all[rand() % 3];
			}
		} else {
			// Original logic for MOV instructions (includes ISZ_8)
			if (reg1 >= 8 || reg2 >= 8) {
				size = valid_sizes[rand() % 3];
			} else {
				size = valid_sizes_all[rand() % 4];
			}
		}
        
        // Generate random immediate value
        int imm_val = rand() % 65536;
        
        // Generate random displacement for memory operations
        long displacement = 0;
        if (instr_type == INSTR_REG_TO_MEM || instr_type == INSTR_MEM_TO_REG || 
            instr_type == INSTR_XADD_MEM || instr_type == INSTR_XCHG_MEM) {
            int disp_type = rand() % num_disp_types;
            switch (disp_type) {
                case DISP_0: displacement = 0; break;
                case DISP_8: displacement = rand() % 128; break;
                case DISP_32: displacement = rand() % 2000; break;
            }
        }
        
        // Random LOCK prefix for XADD/XCHG (50% chance)
        int use_lock = (rand() % 2);
        
        // Generate the instruction based on type
		// Generate the instruction based on type
		switch (instr_type) {
			case INSTR_REG_TO_REG:
				fprintf(stderr, "Generating: MOV R%d->R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_mov_register_to_register(size, reg1, reg2, next_ptr);
				break;
				
			case INSTR_IMM_TO_REG:
				fprintf(stderr, "Generating: MOV #%X->R%d (size=%d)\n", imm_val, reg1, size);
				next_ptr = build_imm_to_register(size, imm_val, reg1, next_ptr);
				break;
				
			case INSTR_REG_TO_MEM:
				fprintf(stderr, "Generating: MOV R%d->[RSI+%ld] (size=%d)\n", reg1, displacement, size);
				next_ptr = build_reg_to_memory(size, reg1, REG_RSI, displacement, next_ptr);
				break;
				
			case INSTR_MEM_TO_REG:
				fprintf(stderr, "Generating: MOV [RSI+%ld]->R%d (size=%d)\n", displacement, reg1, size);
				next_ptr = build_mov_memory_to_register(size, REG_RSI, reg1, displacement, next_ptr);
				break;
				
			case INSTR_XADD_REG:
				// NO LOCK for register-to-register (already atomic within core)
				fprintf(stderr, "Generating: XADD R%d,R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_xadd(size, reg1, reg2, -1, 0, next_ptr);  // use_lock = 0
				break;
				
			case INSTR_XADD_MEM:
				fprintf(stderr, "Generating: %sXADD [RSI+%ld],R%d (size=%d)\n", use_lock ? "LOCK " : "", displacement, reg2, size);
				next_ptr = build_xadd(size, REG_RSI, reg2, displacement, use_lock, next_ptr);
				break;
				
			case INSTR_XCHG_REG:
				// NO LOCK for register-to-register
				fprintf(stderr, "Generating: XCHG R%d,R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_xchg(size, reg1, reg2, -1, 0, next_ptr);  // use_lock = 0
				break;
				
			case INSTR_XCHG_MEM:
				// LOCK makes sense for memory operations
				fprintf(stderr, "Generating: %sXCHG [RSI+%ld],R%d (size=%d)\n", use_lock ? "LOCK " : "", displacement, reg2, size);
				next_ptr = build_xchg(size, REG_RSI, reg2, displacement, use_lock, next_ptr);
				break;
		}
        
        fprintf(stderr, "Instruction %d complete, next_ptr: 0x%lx\n", i+1, (long)next_ptr);
    }
    
    next_ptr = add_endi(next_ptr);
    fprintf(stderr, "Generated %d total instructions\n", num_inst);
    return num_inst;
}