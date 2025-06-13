/*
 * GENERAL Instruction Format
 *
 * -----------------------------------------------------------------
 * | Instructoin    |   Opcode | ModR/M | Displacement | Immediate |
 * | Prefixess       |          |        |              |           |
 * -----------------------------------------------------------------
 *
 *  7  6  5   3  2   0
 * --------------------
 * | Mod | Reg** | R/M |
 * --------------------
 */ 

#include <stdio.h>
#include <stdlib.h>
/*
 * definition we need to support these functions.
 *
 * see Table 2-2 in SDM for register/MODRM encode usage
 *
 *
 */
#define PREFIX_16BIT   0x66
#define PREFIX_32BIT	0x67		// Address size prefix (for 32-bit addressing in 64-bit mode)
#define BASE_MODRM     0xc0
#define REG_SHIFT      0x3
#define MODRM_SHIFT    0x6
#define RM_SHIFT       0x0
#define REG_MASK       0x7
#define RM_MASK        0x7
#define MOD_MASK       0x3

// 64-bit register defs based on MOD RM table
#define REG_RAX        0x0
#define REG_RCX        0x1
#define REG_RDX        0x2
#define REG_RBX        0x3
#define REG_RSP        0x4
#define REG_RBP        0x5
#define REG_RSI        0x6
#define REG_RDI        0x7
// Additional 64-bit registers (R8-R15) - same encoding as RAX-RDI, differentiated by REX bits
// Additional 64-bit registers (R8-R15) - use values 8-15
#define REG_R8         8
#define REG_R9         9
#define REG_R10        10
#define REG_R11        11
#define REG_R12        12
#define REG_R13        13
#define REG_R14        14
#define REG_R15        15

// byte offset
#define BYTE1_OFF      0x1
#define BYTE2_OFF      0x2
#define BYTE3_OFF      0x3
#define BYTE4_OFF      0x4
// Additional byte offsets for 64-bit immediate values only
#define BYTE5_OFF      0x5
#define BYTE6_OFF      0x6
#define BYTE7_OFF      0x7
#define BYTE8_OFF      0x8

// ISIZE (instruction size in bytes, for move example 2byte = 16bit)

#define ISZ_1         0x1
#define ISZ_2         0x2
#define ISZ_4         0x4
#define ISZ_8         0x8

// REX prefix definitions
#define REX_BASE		   0x40
#define REX_W          0x08    // REX.W - 64-bit operand size
#define REX_R          0x04    // REX.R - Extension of ModR/M reg field
#define REX_X          0x02    // REX.X - Extension of SIB index field  
#define REX_B          0x01    // REX.B - Extension of ModR/M r/m field


// code generation defines

#define MAX_INSTR_BYTES 10000
#define MAX_DATA_BYTE  (10*1024)  // allocate 10K
/*
 * Function: build_mov_register_to_register
 *
 * Description:
 *
 * Inputs: 
 *
 *  short mov_size               :  size of the move being requested
 *  int   src_reg                :  register sorce encoding 
 *  int   dest_reg               :  destination reg of move
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */


static inline volatile char *build_mov_register_to_register(short mov_size, int src_reg, int dest_reg, volatile char *tgt_addr)
{
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Check if we need REX prefix
    if (mov_size == 8) {
        rex_prefix |= REX_W;  // 64-bit operand size
        need_rex = 1;
    }
    if (dest_reg >= 8) {
        rex_prefix |= REX_R;  // Extended destination register
        need_rex = 1;
    }
    if (src_reg >= 8) {
        rex_prefix |= REX_B;  // Extended source register  
        need_rex = 1;
    }
    
    // Special case: 8-bit operations on RSP, RBP, RSI, RDI need REX to access SPL, BPL, SIL, DIL
    if (mov_size == 1 && (src_reg == REG_RSP || src_reg == REG_RBP || src_reg == REG_RSI || src_reg == REG_RDI ||
                          dest_reg == REG_RSP || dest_reg == REG_RBP || dest_reg == REG_RSI || dest_reg == REG_RDI)) {
        need_rex = 1;  // Just REX_BASE, no additional bits needed
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (mov_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }

    // Use low 3 bits of register values for ModR/M encoding
    int src_reg_bits = src_reg & 0x7;
    int dest_reg_bits = dest_reg & 0x7;

    // now lets look at each size and determine which opcode required
    switch(mov_size) {

    case 1: 
        (*(short *) tgt_addr) = ((BASE_MODRM) + (dest_reg_bits << REG_SHIFT) + src_reg_bits) << 8 | 0x8a;
        tgt_addr += BYTE2_OFF;
        break;

    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
    case 8:  // 64-bit uses same opcode as 32-bit, but with REX.W prefix
        (*(short *) tgt_addr) = ((BASE_MODRM) + (dest_reg_bits << REG_SHIFT) + src_reg_bits) << 8 | 0x8b;
        tgt_addr += BYTE2_OFF;
        break;

    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to register to register move\n", mov_size);
        return (NULL);
    }
            
    return(tgt_addr);
}


/*
 * Function: build_imm_to_register
 *
 * Description: Build MOV immediate to register instruction using C6/C7 opcode format
 *
 * Inputs: 
 *
 *  short mov_size               :  size of the move being requested (1, 2, or 4 bytes)
 *  int   immediate_val          :  immediate value to move
 *  int   dest_reg               :  destination register encoding 
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */
static inline volatile char *build_imm_to_register(short mov_size, long immediate_val, int dest_reg, volatile char *tgt_addr)
{
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Check if we need REX prefix
    if (mov_size == 8) {
        rex_prefix |= REX_W;  // 64-bit operand size
        need_rex = 1;
    }
    if (dest_reg >= 8) {
        rex_prefix |= REX_B;  // Extended destination register
        need_rex = 1;
    }
    
    // Special case: 8-bit operations on RSP, RBP, RSI, RDI need REX to access SPL, BPL, SIL, DIL
    if (mov_size == 1 && (dest_reg == REG_RSP || dest_reg == REG_RBP || dest_reg == REG_RSI || dest_reg == REG_RDI)) {
        need_rex = 1;  // Just REX_BASE, no additional bits needed
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (mov_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }
    
    // Use low 3 bits of register values for ModR/M encoding
    int dest_reg_bits = dest_reg & 0x7;
    
    // now lets look at each size and determine which opcode required
    switch(mov_size) {
    case 1: 
        // C6 /0 - MOV r/m8, imm8
        (*(short *) tgt_addr) = ((BASE_MODRM) + (0 << REG_SHIFT) + dest_reg_bits) << 8 | 0xC6;
        tgt_addr += BYTE2_OFF;
        *tgt_addr = (char)(immediate_val & 0xFF);
        tgt_addr++;
        break;
    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
        (*(short *) tgt_addr) = ((BASE_MODRM) + (0 << REG_SHIFT) + dest_reg_bits) << 8 | 0xC7;
        tgt_addr += BYTE2_OFF;
        
        if (mov_size == 2) {
            *(short *)tgt_addr = (short)(immediate_val & 0xFFFF);
            tgt_addr += BYTE2_OFF;
        } else { // mov_size == 4 or mov_size == 8
            // For 64-bit, immediate is 32-bit and gets sign-extended
            *(int *)tgt_addr = (int)(immediate_val & 0xFFFFFFFF);
            tgt_addr += BYTE4_OFF;
        }
        break;
	case 8:  // this requires rex.w to be set prior to this..
		 (*(char *) tgt_addr) = (0xb8 + dest_reg_bits);      
		 tgt_addr += BYTE1_OFF;
		 (*(long *)tgt_addr) = immediate_val;
		 tgt_addr +=8;
		 break;
    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to immediate to register move\n", mov_size);
        return (NULL);
    }
            
    return(tgt_addr);
}
/*
 * Function: build_reg_to_memory
 *
 * Description: Build MOV register to memory instruction with displacement support
 *
 * Inputs: 
 *
 *  short mov_size               :  size of the move being requested (1, 2, or 4 bytes)
 *  int   src_reg                :  source register encoding (data to move)
 *  int   mem_base_reg           :  base register encoding (memory address pointer)
 *  long  displacement           :  displacement value to add to base register  
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 * Example: build_reg_to_memory(ISZ_4, REG_EBX, REG_ESI, 8, ptr) generates "MOV [ESI+8], EBX"
 */
static inline volatile char *build_reg_to_memory(short mov_size, int src_reg, int mem_base_reg, long displacement, volatile char *tgt_addr)
{
    int mod_value;
    int disp_bytes;
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Determine MOD field and displacement bytes based on displacement value
    if (displacement == 0) {
        mod_value = 0x00;  // MOD=00, no displacement
        disp_bytes = 0;
    } else if (displacement >= -128 && displacement <= 127) {
        mod_value = 0x40;  // MOD=01, 8-bit displacement
        disp_bytes = 1;
    } else {
        mod_value = 0x80;  // MOD=10, 32-bit displacement  
        disp_bytes = 4;
    }
    
    // Check if we need REX prefix
    if (mov_size == 8) {
        rex_prefix |= REX_W;  // 64-bit operand size
        need_rex = 1;
    }
    if (src_reg >= 8) {
        rex_prefix |= REX_R;  // Extended source register
        need_rex = 1;
    }
    if (mem_base_reg >= 8) {
        rex_prefix |= REX_B;  // Extended base register
        need_rex = 1;
    }
    
    // Special case: 8-bit operations on RSP, RBP, RSI, RDI need REX to access SPL, BPL, SIL, DIL
    if (mov_size == 1 && (src_reg == REG_RSP || src_reg == REG_RBP || src_reg == REG_RSI || src_reg == REG_RDI ||
                          mem_base_reg == REG_RSP || mem_base_reg == REG_RBP || mem_base_reg == REG_RSI || mem_base_reg == REG_RDI)) {
        need_rex = 1;  // Just REX_BASE, no additional bits needed
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (mov_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }
    
    // Use low 3 bits of register values for ModR/M encoding
    int src_reg_bits = src_reg & 0x7;
    int mem_base_reg_bits = mem_base_reg & 0x7;
    
    // now lets look at each size and determine which opcode required
    switch(mov_size) {
    case 1: 
        // 88 /r - MOV r/m8, r8
        // ModR/M: Variable MOD, REG=src_reg, R/M=mem_base_reg
        (*(short *) tgt_addr) = (mod_value + (src_reg_bits << REG_SHIFT) + mem_base_reg_bits) << 8 | 0x88;
        fprintf(stderr, "ISZ_1 reg-to-mem: stored at 0x%lx: 0x%04x, disp=%ld\n", (long)tgt_addr, *(short *)tgt_addr, displacement);
        tgt_addr += BYTE2_OFF;
        break;
    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
    case 8:  // 64-bit uses same opcode as 32-bit, but with REX.W prefix
        // 89 /r - MOV r/m16/32/64, r16/32/64 
        // ModR/M: Variable MOD, REG=src_reg, R/M=mem_base_reg
        (*(short *) tgt_addr) = (mod_value + (src_reg_bits << REG_SHIFT) + mem_base_reg_bits) << 8 | 0x89;
        fprintf(stderr, "ISZ_%d reg-to-mem: stored at 0x%lx: 0x%04x, disp=%ld\n", mov_size, (long)tgt_addr, *(short *)tgt_addr, displacement);
        tgt_addr += BYTE2_OFF;
        break;
    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to register to memory move\n", mov_size);
        return (NULL);
    }
    
    // Emit displacement bytes based on MOD field
    if (disp_bytes == 1) {
        *tgt_addr = (char)(displacement & 0xFF);
        tgt_addr += BYTE1_OFF;
        fprintf(stderr, "  + 8-bit displacement: 0x%02x\n", (unsigned char)(displacement & 0xFF));
    } else if (disp_bytes == 4) {
        *(int *)tgt_addr = (int)displacement;
        tgt_addr += BYTE4_OFF;
        fprintf(stderr, "  + 32-bit displacement: 0x%08x\n", (int)displacement);
    }
            
    return(tgt_addr);
}

/*
 * Function: build_mov_memory_to_register
 *
 * Description: Build MOV memory to register instruction with displacement support
 *
 * Inputs: 
 *
 *  short mov_size               :  size of the move being requested (1, 2, or 4 bytes)
 *  int   mem_base_reg           :  base register encoding (memory address pointer)
 *  int   dest_reg               :  destination register encoding (where data goes)
 *  long  displacement           :  displacement value to add to base register
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 * Example: build_mov_memory_to_register(ISZ_4, REG_ESI, REG_EAX, 8, ptr) generates "MOV EAX, [ESI+8]"
 */
static inline volatile char *build_mov_memory_to_register(short mov_size, int mem_base_reg, int dest_reg, long displacement, volatile char *tgt_addr)
{
    int mod_value;
    int disp_bytes;
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Determine MOD field and displacement bytes based on displacement value
    if (displacement == 0) {
        mod_value = 0x00;  // MOD=00, no displacement
        disp_bytes = 0;
    } else if (displacement >= -128 && displacement <= 127) {
        mod_value = 0x40;  // MOD=01, 8-bit displacement
        disp_bytes = 1;
    } else {
        mod_value = 0x80;  // MOD=10, 32-bit displacement  
        disp_bytes = 4;
    }
    
    // Check if we need REX prefix
    if (mov_size == 8) {
        rex_prefix |= REX_W;  // 64-bit operand size
        need_rex = 1;
    }
    if (dest_reg >= 8) {
        rex_prefix |= REX_R;  // Extended destination register
        need_rex = 1;
    }
    if (mem_base_reg >= 8) {
        rex_prefix |= REX_B;  // Extended base register
        need_rex = 1;
    }
    
    // Special case: 8-bit operations on RSP, RBP, RSI, RDI need REX to access SPL, BPL, SIL, DIL
    if (mov_size == 1 && (dest_reg == REG_RSP || dest_reg == REG_RBP || dest_reg == REG_RSI || dest_reg == REG_RDI ||
                          mem_base_reg == REG_RSP || mem_base_reg == REG_RBP || mem_base_reg == REG_RSI || mem_base_reg == REG_RDI)) {
        need_rex = 1;  // Just REX_BASE, no additional bits needed
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (mov_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }
    
    // Use low 3 bits of register values for ModR/M encoding
    int dest_reg_bits = dest_reg & 0x7;
    int mem_base_reg_bits = mem_base_reg & 0x7;
    
    // now lets look at each size and determine which opcode required
    switch(mov_size) {
    case 1: 
        // 8A /r - MOV r8, r/m8
        // ModR/M: Variable MOD, REG=dest_reg, R/M=mem_base_reg
        (*(short *) tgt_addr) = (mod_value + (dest_reg_bits << REG_SHIFT) + mem_base_reg_bits) << 8 | 0x8A;
        fprintf(stderr, "ISZ_1 mem-to-reg: stored at 0x%lx: 0x%04x, disp=%ld\n", (long)tgt_addr, *(short *)tgt_addr, displacement);
        tgt_addr += BYTE2_OFF;
        break;
    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
    case 8:  // 64-bit uses same opcode as 32-bit, but with REX.W prefix
        // 8B /r - MOV r16/32/64, r/m16/32/64
        // ModR/M: Variable MOD, REG=dest_reg, R/M=mem_base_reg
        (*(short *) tgt_addr) = (mod_value + (dest_reg_bits << REG_SHIFT) + mem_base_reg_bits) << 8 | 0x8B;
        fprintf(stderr, "ISZ_%d mem-to-reg: stored at 0x%lx: 0x%04x, disp=%ld\n", mov_size, (long)tgt_addr, *(short *)tgt_addr, displacement);
        tgt_addr += BYTE2_OFF;
        break;
    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to memory to register move\n", mov_size);
        return (NULL);
    }
    
    // Emit displacement bytes based on MOD field
    if (disp_bytes == 1) {
        *tgt_addr = (char)(displacement & 0xFF);
        tgt_addr += BYTE1_OFF;
        fprintf(stderr, "  + 8-bit displacement: 0x%02x\n", (unsigned char)(displacement & 0xFF));
    } else if (disp_bytes == 4) {
        *(int *)tgt_addr = (int)displacement;
        tgt_addr += BYTE4_OFF;
        fprintf(stderr, "  + 32-bit displacement: 0x%08x\n", (int)displacement);
    }
            
    return(tgt_addr);
}

/*
 * Function: build_xadd
 *
 * Description: Build XADD r/m, r instruction (exchange and add - atomic add with return of old value)
 *              with optional LOCK prefix for atomic operations
 *              XADD performs: temp=dest, dest=dest+src, src=temp
 *
 * Inputs: 
 *
 *  short xadd_size              :  size of the operation being requested (1, 2, or 4 bytes)
 *  int   rm_reg                 :  r/m operand register (if displacement==-1) or base register (for memory)
 *  int   reg                    :  register operand (always a register) - source value
 *  long  displacement           :  displacement value (-1 for reg-to-reg, >=0 for memory addressing)
 *  int   use_lock               :  1 = add LOCK prefix (0xF0), 0 = no LOCK prefix
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 * Examples: 
 *   build_xadd_reg_mem(4, REG_EBX, REG_EAX, -1, 0, ptr)     → "XADD EBX, EAX" (reg-to-reg)
 *   build_xadd_reg_mem(4, REG_ESI, REG_EAX, 8, 0, ptr)      → "XADD [ESI+8], EAX" (mem-to-reg)
 *   build_xadd_reg_mem(4, REG_ESI, REG_EAX, 8, 1, ptr)      → "LOCK XADD [ESI+8], EAX" (atomic)
 */
static inline volatile char *build_xadd(short xadd_size, int rm_reg, int reg, long displacement, int use_lock, volatile char *tgt_addr)
{
    int mod_value;
    int disp_bytes;
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Emit LOCK prefix if requested (must come before all other prefixes)
    if (use_lock) {
        (*tgt_addr++) = 0xF0;  // LOCK prefix
    }
    
    // Determine if this is register-to-register or register-to-memory
    if (displacement == -1) {
        // Register-to-register: MOD=11, no displacement
        mod_value = 0xC0;  // MOD=11 (BASE_MODRM)
        disp_bytes = 0;
    } else {
        // Register-to-memory: determine MOD field based on displacement value
        if (displacement == 0) {
            mod_value = 0x00;  // MOD=00, no displacement
            disp_bytes = 0;
        } else if (displacement >= -128 && displacement <= 127) {
            mod_value = 0x40;  // MOD=01, 8-bit displacement
            disp_bytes = 1;
        } else {
            mod_value = 0x80;  // MOD=10, 32-bit displacement  
            disp_bytes = 4;
        }
    }
    
    // Check if we need REX prefix
    if (reg >= 8) {
        rex_prefix |= REX_R;  // Extended register operand
        need_rex = 1;
    }
    if (rm_reg >= 8) {
        rex_prefix |= REX_B;  // Extended r/m operand
        need_rex = 1;
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (xadd_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }
    
    // Use low 3 bits of register values for ModR/M encoding
    int reg_bits = reg & 0x7;
    int rm_reg_bits = rm_reg & 0x7;
    
    // XADD is a two-byte opcode starting with 0x0F
    (*tgt_addr++) = 0x0F;  // First byte of XADD opcode
    
    // now lets look at each size and determine which opcode required
    switch(xadd_size) {
    case 1: 
        // 0F C0 /r - XADD r/m8, r8
        // ModR/M: Variable MOD, REG=reg, R/M=rm_reg
        (*(short *) tgt_addr) = (mod_value + (reg_bits << REG_SHIFT) + rm_reg_bits) << 8 | 0xC0;
        if (displacement == -1) {
            fprintf(stderr, "%sISZ_1 reg-to-reg XADD: R%d += R%d (and exchange)\n", use_lock ? "LOCK " : "", rm_reg, reg);
        } else {
            fprintf(stderr, "%sISZ_1 mem-to-reg XADD: [R%d+%ld] += R%d (and exchange)\n", use_lock ? "LOCK " : "", rm_reg, displacement, reg);
        }
        tgt_addr += BYTE2_OFF;
        break;
    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
        // 0F C1 /r - XADD r/m16/32, r16/32
        // ModR/M: Variable MOD, REG=reg, R/M=rm_reg
        (*(short *) tgt_addr) = (mod_value + (reg_bits << REG_SHIFT) + rm_reg_bits) << 8 | 0xC1;
        if (displacement == -1) {
            fprintf(stderr, "%sISZ_%d reg-to-reg XADD: R%d += R%d (and exchange)\n", use_lock ? "LOCK " : "", xadd_size, rm_reg, reg);
        } else {
            fprintf(stderr, "%sISZ_%d mem-to-reg XADD: [R%d+%ld] += R%d (and exchange)\n", use_lock ? "LOCK " : "", xadd_size, rm_reg, displacement, reg);
        }
        tgt_addr += BYTE2_OFF;
        break;
    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to XADD instruction\n", xadd_size);
        return (NULL);
    }
    
    // Emit displacement bytes based on MOD field (only for memory operations)
    if (displacement != -1) {
        if (disp_bytes == 1) {
            *tgt_addr = (char)(displacement & 0xFF);
            tgt_addr += BYTE1_OFF;
            fprintf(stderr, "  + 8-bit displacement: 0x%02x\n", (unsigned char)(displacement & 0xFF));
        } else if (disp_bytes == 4) {
            *(int *)tgt_addr = (int)displacement;
            tgt_addr += BYTE4_OFF;
            fprintf(stderr, "  + 32-bit displacement: 0x%08x\n", (int)displacement);
        }
    }
            
    return(tgt_addr);
}


/*
 * Function: build_xchg
 *
 * Description: Build XCHG r/m, r instruction (can exchange register with register or memory)
 *              with optional LOCK prefix for atomic operations
 *
 * Inputs: 
 *
 *  short xchg_size              :  size of the exchange being requested (1, 2, or 4 bytes)
 *  int   rm_reg                 :  r/m operand register (if displacement==-1) or base register (for memory)
 *  int   reg                    :  register operand (always a register)
 *  long  displacement           :  displacement value (-1 for reg-to-reg, >=0 for memory addressing)
 *  int   use_lock               :  1 = add LOCK prefix (0xF0), 0 = no LOCK prefix
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 * Examples: 
 *   build_xchg_reg_mem(4, REG_EBX, REG_EAX, -1, 0, ptr)     → "XCHG EBX, EAX" (reg-to-reg)
 *   build_xchg_reg_mem(4, REG_ESI, REG_EAX, 8, 0, ptr)      → "XCHG [ESI+8], EAX" (mem-to-reg)
 *   build_xchg_reg_mem(4, REG_ESI, REG_EAX, 8, 1, ptr)      → "LOCK XCHG [ESI+8], EAX" (atomic)
 */
static inline volatile char *build_xchg(short xchg_size, int rm_reg, int reg, long displacement, int use_lock, volatile char *tgt_addr)
{
    int mod_value;
    int disp_bytes;
    unsigned char rex_prefix = 0;
    int need_rex = 0;
    
    // Emit LOCK prefix if requested (must come before all other prefixes)
    if (use_lock) {
        (*tgt_addr++) = 0xF0;  // LOCK prefix
    }
    
    // Determine if this is register-to-register or register-to-memory
    if (displacement == -1) {
        // Register-to-register: MOD=11, no displacement
        mod_value = 0xC0;  // MOD=11 (BASE_MODRM)
        disp_bytes = 0;
    } else {
        // Register-to-memory: determine MOD field based on displacement value
        if (displacement == 0) {
            mod_value = 0x00;  // MOD=00, no displacement
            disp_bytes = 0;
        } else if (displacement >= -128 && displacement <= 127) {
            mod_value = 0x40;  // MOD=01, 8-bit displacement
            disp_bytes = 1;
        } else {
            mod_value = 0x80;  // MOD=10, 32-bit displacement  
            disp_bytes = 4;
        }
    }
    
    // Check if we need REX prefix
    if (reg >= 8) {
        rex_prefix |= REX_R;  // Extended register operand
        need_rex = 1;
    }
    if (rm_reg >= 8) {
        rex_prefix |= REX_B;  // Extended r/m operand
        need_rex = 1;
    }
    
    // Emit REX prefix if needed
    if (need_rex) {
        (*tgt_addr++) = REX_BASE | rex_prefix;
    }
    
    // for 16 bit mode we need to treat it special because it requires a prefix
    if (xchg_size == 2) {
        (*tgt_addr++) = PREFIX_16BIT;
    }
    
    // Use low 3 bits of register values for ModR/M encoding
    int reg_bits = reg & 0x7;
    int rm_reg_bits = rm_reg & 0x7;
    
    // now lets look at each size and determine which opcode required
    switch(xchg_size) {
    case 1: 
        // 86 /r - XCHG r/m8, r8
        // ModR/M: Variable MOD, REG=reg, R/M=rm_reg
        (*(short *) tgt_addr) = (mod_value + (reg_bits << REG_SHIFT) + rm_reg_bits) << 8 | 0x86;
        if (displacement == -1) {
            fprintf(stderr, "%sISZ_1 reg-to-reg XCHG: R%d <-> R%d\n", use_lock ? "LOCK " : "", rm_reg, reg);
        } else {
            fprintf(stderr, "%sISZ_1 mem-to-reg XCHG: [R%d+%ld] <-> R%d\n", use_lock ? "LOCK " : "", rm_reg, displacement, reg);
        }
        tgt_addr += BYTE2_OFF;
        break;
    case 2:  // can overload this case because same opcode, but already set prefix
    case 4: 
        // 87 /r - XCHG r/m16/32, r16/32
        // ModR/M: Variable MOD, REG=reg, R/M=rm_reg
        (*(short *) tgt_addr) = (mod_value + (reg_bits << REG_SHIFT) + rm_reg_bits) << 8 | 0x87;
        if (displacement == -1) {
            fprintf(stderr, "%sISZ_%d reg-to-reg XCHG: R%d <-> R%d\n", use_lock ? "LOCK " : "", xchg_size, rm_reg, reg);
        } else {
            fprintf(stderr, "%sISZ_%d mem-to-reg XCHG: [R%d+%ld] <-> R%d\n", use_lock ? "LOCK " : "", xchg_size, rm_reg, displacement, reg);
        }
        tgt_addr += BYTE2_OFF;
        break;
    default:
        fprintf(stderr,"ERROR: Incorrect size (%d) passed to XCHG instruction\n", xchg_size);
        return (NULL);
    }
    
    // Emit displacement bytes based on MOD field (only for memory operations)
    if (displacement != -1) {
        if (disp_bytes == 1) {
            *tgt_addr = (char)(displacement & 0xFF);
            tgt_addr += BYTE1_OFF;
            fprintf(stderr, "  + 8-bit displacement: 0x%02x\n", (unsigned char)(displacement & 0xFF));
        } else if (disp_bytes == 4) {
            *(int *)tgt_addr = (int)displacement;
            tgt_addr += BYTE4_OFF;
            fprintf(stderr, "  + 32-bit displacement: 0x%08x\n", (int)displacement);
        }
    }
            
    return(tgt_addr);
}

/*
 * Function: build_mfence
 *
 * Description: Build MFENCE instruction (Memory Fence - full memory barrier)
 *              Serializes all memory operations (loads and stores)
 *
 * Inputs: 
 *
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */
static inline volatile char *build_mfence(volatile char *tgt_addr)
{
    // MFENCE: 0x0F 0xAE 0xF0
    (*tgt_addr++) = 0x0F;  // Two-byte opcode escape prefix
    (*tgt_addr++) = 0xAE;  // Second byte of fence opcode
    (*tgt_addr++) = 0xF0;  // MFENCE specific byte
    
    fprintf(stderr, "Generated MFENCE (full memory barrier)\n");
    
    return(tgt_addr);
}

/*
 * Function: build_sfence
 *
 * Description: Build SFENCE instruction (Store Fence - store memory barrier)
 *              Serializes all store operations before the fence with stores after
 *
 * Inputs: 
 *
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */
static inline volatile char *build_sfence(volatile char *tgt_addr)
{
    // SFENCE: 0x0F 0xAE 0xF8  
    (*tgt_addr++) = 0x0F;  // Two-byte opcode escape prefix
    (*tgt_addr++) = 0xAE;  // Second byte of fence opcode
    (*tgt_addr++) = 0xF8;  // SFENCE specific byte
    
    fprintf(stderr, "Generated SFENCE (store memory barrier)\n");
    
    return(tgt_addr);
}

/*
 * Function: build_lfence
 *
 * Description: Build LFENCE instruction (Load Fence - load memory barrier)
 *              Serializes all load operations before the fence with loads after
 *
 * Inputs: 
 *
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */
static inline volatile char *build_lfence(volatile char *tgt_addr)
{
    // LFENCE: 0x0F 0xAE 0xE8
    (*tgt_addr++) = 0x0F;  // Two-byte opcode escape prefix  
    (*tgt_addr++) = 0xAE;  // Second byte of fence opcode
    (*tgt_addr++) = 0xE8;  // LFENCE specific byte
    
    fprintf(stderr, "Generated LFENCE (load memory barrier)\n");
    
    return(tgt_addr);
}

/*
 * Function: build_push_reg
 *
 * Description:
 *
 * builds push register
 *
 * Inputs: 
 *
 *  int reg_index                :  index of register
 *  int x86_64f                  :  flag to indicate if we need to extend to rex format
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */


static inline volatile char *build_push_reg(int reg_index, int x86_64f, volatile char *tgt_addr)
{

	if (x86_64f) {
		// this is a quick hack for REX_B prefix

		(*(char *) tgt_addr)=(REX_BASE | REX_B);
		tgt_addr += BYTE1_OFF;
	}

	(*(char *) tgt_addr) = 0x50 + (reg_index & 0x7);
	tgt_addr += BYTE1_OFF;
			
        return(tgt_addr);
}

/*
 * Function: build_pop_reg
 *
 * Description:
 *
 * builds pop register
 *
 * Inputs: 
 *
 *  int reg_index                :  index of register
 *  int x86_64f                  :  flag to indicate if we need to exted to rex format
 *  volatile char *tgt_addr      :  starting memory address of where to store instruction
 *
 * Output: 
 *
 *  returns adjusted address after encoding instruction
 *
 */


static inline volatile char *build_pop_reg(int reg_index, int x86_64f, volatile char *tgt_addr)
{

	if (x86_64f) {
		// this is a quick hack for REX_B prefix

		(*(char *) tgt_addr)=(REX_BASE | REX_B);
		tgt_addr += BYTE1_OFF;
	}

	(*(char *) tgt_addr) = 0x58 + (reg_index & 0x7);
	tgt_addr += BYTE1_OFF;
			
        return(tgt_addr);
}
