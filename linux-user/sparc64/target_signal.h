#ifndef SPARC64_TARGET_SIGNAL_H
#define SPARC64_TARGET_SIGNAL_H

/* this struct defines a stack used during syscall handling */

typedef struct target_sigaltstack {
	abi_ulong ss_sp;
	abi_long ss_flags;
	abi_ulong ss_size;
} target_stack_t;


/*
 * sigaltstack controls
 */
#define TARGET_SS_ONSTACK	1
#define TARGET_SS_DISABLE	2

#define TARGET_MINSIGSTKSZ	4096
#define TARGET_SIGSTKSZ		16384

#define TARGET_ARCH_HAS_SETUP_FRAME
#endif /* SPARC64_TARGET_SIGNAL_H */
