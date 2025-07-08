//
// simple encoding example for IA-32 validation project
//
// Project Part3: added multple processing supports
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>  // for strcpy, strlen

#include <limits.h>    /* for PAGESIZE */

#include <sched.h> 
#include "ia32_encode.h"
  

// globals to aid debug to start
volatile char *mptr=0,*next_ptr=0,*mdptr=0, *comm_ptr=0;
int num_inst=0,i=0;
int target_ninstrs=MAX_DEF_INSTRS;
int nthreads=1,pid_task[MAX_THREADS],pid=0;
unsigned seed = 12345;
FILE *logfile = NULL;

typedef struct { 
	volatile unsigned long *pointer_addr;
} test_i;

test_i test_info [NUM_PTRS];
//
// declarations for holding thread information
//
typedef volatile unsigned long *tptrs;

volatile unsigned long *mptr_threads[MAX_THREADS];
volatile unsigned long *mdptr_threads[MAX_THREADS];
volatile unsigned long *comm_ptr_threads[MAX_THREADS];



// 
// declarations for starting test
//
typedef int (*funct_t)();
funct_t start_test;
int executeit();

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/*
 * simple routine to randomize numbers in a range
 */
int rand_range(int min_n, int max_n)
{
	return rand() % (max_n - min_n + 1) + min_n;
}


main(int argc, char *argv[])
{

	int ibuilt=0;

	/* process arguments here */
	if (argc >= 2) seed = atoi(argv[1]);
	if (argc >= 3) target_ninstrs = atoi(argv[2]);
	if (argc >= 4) nthreads = atoi(argv[3]);

	char logfilename[256] = "";
	

	if (argc >= 5 && strlen(argv[4]) > 0) {
		strcpy(logfilename, argv[4]);
		logfile = fopen(logfilename, "w");
		if (!logfile) {
			fprintf(logfile, "Error: Cannot open log file %s\n", logfilename);
			exit(1);
		}
		printf("Logging to: %s\n", logfilename);
	}

	printf("\nstarting seed = %d\n", seed);
	printf("Number of instructions = %d\n", target_ninstrs);
	printf("Number of threads = %d\n", nthreads);

	if (nthreads > MAX_THREADS) {
		fprintf(logfile,"Sorry only built for %d threads over riding your %d\n", MAX_THREADS, nthreads);
		fflush(logfile);
		nthreads=MAX_THREADS;
	}

	srand(seed);

	/* allocate buffer to perform stores and loads to  */


	test_info[DATA].pointer_addr = mmap(
		(void *) 0,
		(MAX_DATA_BYTES+PAGESIZE-1) * nthreads,
		PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_SHARED,
		0, 0
		);

	/* save the base address for debug like before */

	mdptr=(volatile char *)test_info[DATA].pointer_addr;

	if (((int *)test_info[DATA].pointer_addr) == (int *)-1) {
		perror("Couldn't mmap (MAX_DATA_BYTES)");
		exit(1);
	}


	
	/* allocate buffer to build instructions into */

	test_info[CODE].pointer_addr = mmap(
		(void *) 0,
		(MAX_INSTR_BYTES+PAGESIZE-1) * nthreads,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_SHARED,
		0, 0
		);

	/* keep a copy to the base here */

	mptr=(volatile char *)test_info[CODE].pointer_addr;

	if (((int *)test_info[CODE].pointer_addr) == (int *)-1) {
		perror("Couldn't mmap (MAX_INSTR_BYTES)");
		exit(1);
	}


	/* allocate buffer to build communications area into */

	test_info[COMM].pointer_addr = mmap(
		(void *) 0,
		(MAX_COMM_BYTES+PAGESIZE-1) * nthreads,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_SHARED,
		0, 0
		);

	comm_ptr=(volatile char *)test_info[COMM].pointer_addr;

	if (((int *)test_info[COMM].pointer_addr) == (int *)-1) {
		perror("Couldn't mmap (MAX_INSTR_BYTES)");
		exit(1);
	}

	/* make the standard output and stderrr unbuffered */

	setbuf(stdout, (char *) NULL);
	setbuf(stderr, (char *) NULL);


	/* start appropriate # of threads */

	for (i=0;i<nthreads;i++) 
	{
	
		next_ptr=(mptr+(i*MAX_INSTR_BYTES));          // init next_ptr
		fprintf(logfile,"T%d next_ptr=0x%lx\n",i,(unsigned long)next_ptr);
		fflush(logfile);
		mdptr_threads[i]=(tptrs)mdptr;  // init threads data pointer
		mptr_threads[i]=(tptrs)next_ptr;                     // save ptr per thread
		comm_ptr_threads[i]=(tptrs)comm_ptr;                 // everyone gets the same for now


		/* use fork to start a new child process */

		if((pid=fork()) == 0) {

			fprintf(logfile,"T%d fork\n",i);
			fflush(logfile);

		if (bind_to_cpu(i, getpid()) != 0) {
			exit(1);
		}

			//
			// NOTE:  you could set your sched_setaffinity here...better to make a subroutine to bind
			// 
			//
			ibuilt=build_instructions(mptr_threads[i],i,logfile);  // build instructions

			/* ok now that I built the critters, time to execute them */

			start_test=(funct_t) mptr_threads[i];
			executeit(start_test);
			fprintf(logfile,"T%d generation program complete, instructions generated: %d\n",i, ibuilt);
			fflush(logfile);

			break;
			
		}
		
                else if (pid_task[i] == -1) {
			perror("fork me failed");
			exit(1);
		} else { // this should be the parent 

			pid_task[i]=pid; // save pid

			fprintf(logfile,"child T%d started:\n",pid);
			fflush(logfile);

		}
	     
	} // end for nthreads


	// wait for threads to complete

	for (i=0;i<nthreads;i++) {
		waitpid(pid_task[i], NULL, 0);
	}


	// clean up the allocation before getting out

	munmap((caddr_t)mdptr,(MAX_DATA_BYTES+PAGESIZE-1)*nthreads);
	munmap((caddr_t)mptr,(MAX_INSTR_BYTES+PAGESIZE-1)*nthreads);
	munmap((caddr_t)comm_ptr,(MAX_COMM_BYTES+PAGESIZE-1)*nthreads);

	// Close log file
	if (logfile) {
		fclose(logfile);
	}

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

int bind_to_cpu(int thread_id, pid_t pid) {
    cpu_set_t mask;
    
    CPU_ZERO(&mask);                    // Clear all CPU bits
    CPU_SET(thread_id, &mask);          // Set bit for specific CPU
    
    if (sched_setaffinity(pid, sizeof(mask), &mask) == -1) {
        perror("sched_setaffinity failed");
        return -1;
    }
    
	fprintf(logfile, "Assigned process %d to CPU_%d\n", pid, thread_id);
	fflush(logfile);

	    cpu_set_t verify_mask;
    if (sched_getaffinity(pid, sizeof(verify_mask), &verify_mask) == 0) {
		fprintf(logfile,"Verified: Process %d bound to CPU_%d = %s\n", pid, thread_id, CPU_ISSET(thread_id, &verify_mask) ? "SUCCESS" : "FAILED");
		fflush(logfile);
    }
    return 0;
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
// INPUTS: next_ptr, thread_id, logfile
// 
// OUTPUT: returns the number of instructions built
// 
int build_instructions(volatile char *next_ptr, int thread_id, FILE *logfile) {

	// Helper macro for logging to both stderr and logfile
	#define LOG_AND_PRINT(format, ...) do { \
		fprintf(stderr, "T%d: " format, thread_id, ##__VA_ARGS__); \
		fflush(stderr); \
		if (logfile) { \
			fprintf(logfile, "T%d: " format, thread_id, ##__VA_ARGS__); \
			fflush(logfile); \
		} \
	} while(0)

	int instructions_built = 0;

	// example instruction generation..

	LOG_AND_PRINT("building instructions\n");

	srand(seed + thread_id);  // Different seed per thread
    
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
        INSTR_XCHG_MEM = 7,      // XCHG reg-to-memory
        INSTR_MFENCE = 8,        // MFENCE - full memory barrier
        INSTR_SFENCE = 9,        // SFENCE - store memory barrier  
        INSTR_LFENCE = 10        // LFENCE - load memory barrier
    };
    int num_instr_types = 11;
    
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
    next_ptr = build_imm_to_register(ISZ_8, (long)mdptr_threads[thread_id], REG_RSI, next_ptr);
    LOG_AND_PRINT("MOVING MDPTR: MOV #%lX->R%d (size=%d)\n", (long)mdptr_threads[thread_id], REG_RSI, ISZ_8);
    instructions_built++;
    LOG_AND_PRINT("Setup: loaded mdptr into RSI\n");
    
    int i;
    // Generate random instructions
    for (i = 0; i < target_ninstrs; i++) {
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

		// Fence instructions don't need size, but we'll set a default
		if (instr_type >= INSTR_MFENCE && instr_type <= INSTR_LFENCE) {
			size = ISZ_4;  // Default size for fence instructions (not actually used)
		}
		// Special handling for XADD/XCHG (no ISZ_8 support)
		else if (instr_type >= INSTR_XADD_REG && instr_type <= INSTR_XCHG_MEM) {
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
		switch (instr_type) {
			case INSTR_REG_TO_REG:
				LOG_AND_PRINT("Generating: MOV R%d->R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_mov_register_to_register(size, reg1, reg2, next_ptr);
				break;
				
			case INSTR_IMM_TO_REG:
				LOG_AND_PRINT("Generating: MOV #%X->R%d (size=%d)\n", imm_val, reg1, size);
				next_ptr = build_imm_to_register(size, imm_val, reg1, next_ptr);
				break;
				
			case INSTR_REG_TO_MEM:
				LOG_AND_PRINT("Generating: MOV R%d->[RSI+%ld] (size=%d)\n", reg1, displacement, size);
				next_ptr = build_reg_to_memory(size, reg1, REG_RSI, displacement, next_ptr);
				break;
				
			case INSTR_MEM_TO_REG:
				LOG_AND_PRINT("Generating: MOV [RSI+%ld]->R%d (size=%d)\n", displacement, reg1, size);
				next_ptr = build_mov_memory_to_register(size, REG_RSI, reg1, displacement, next_ptr);
				break;
				
			case INSTR_XADD_REG:
				// NO LOCK for register-to-register (already atomic within core)
				LOG_AND_PRINT("Generating: XADD R%d,R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_xadd(size, reg1, reg2, -1, 0, next_ptr);  // use_lock = 0
				break;
				
			case INSTR_XADD_MEM:
				LOG_AND_PRINT("Generating: %sXADD [RSI+%ld],R%d (size=%d)\n", use_lock ? "LOCK " : "", displacement, reg2, size);
				next_ptr = build_xadd(size, REG_RSI, reg2, displacement, use_lock, next_ptr);
				break;
				
			case INSTR_XCHG_REG:
				// NO LOCK for register-to-register
				LOG_AND_PRINT("Generating: XCHG R%d,R%d (size=%d)\n", reg1, reg2, size);
				next_ptr = build_xchg(size, reg1, reg2, -1, 0, next_ptr);  // use_lock = 0
				break;
				
			case INSTR_XCHG_MEM:
				// LOCK makes sense for memory operations
				LOG_AND_PRINT("Generating: %sXCHG [RSI+%ld],R%d (size=%d)\n", use_lock ? "LOCK " : "", displacement, reg2, size);
				next_ptr = build_xchg(size, REG_RSI, reg2, displacement, use_lock, next_ptr);
				break;
				
			case INSTR_MFENCE:
				LOG_AND_PRINT("Generating: MFENCE (full memory barrier)\n");
				next_ptr = build_mfence(next_ptr);
				break;
				
			case INSTR_SFENCE:
				LOG_AND_PRINT("Generating: SFENCE (store memory barrier)\n");
				next_ptr = build_sfence(next_ptr);
				break;
				
			case INSTR_LFENCE:
				LOG_AND_PRINT("Generating: LFENCE (load memory barrier)\n");
				next_ptr = build_lfence(next_ptr);
				break;
		}
        
        instructions_built++;
        LOG_AND_PRINT("Instruction %d complete, next_ptr: 0x%lx\n", instructions_built, (long)next_ptr);
    }

	LOG_AND_PRINT("next ptr is now 0x%lx\n", (long) next_ptr);

    next_ptr = add_endi(next_ptr);
    LOG_AND_PRINT("Generated %d total instructions\n", instructions_built);
    return instructions_built;

}