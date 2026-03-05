// BEGIN PREAMBLE
	.version	3.1
	.target	sm_35
	.address_size 64
// END PREAMBLE

// BEGIN GLOBAL FUNCTION DECL: __gbl_ctors
.extern .func __gbl_ctors;

// BEGIN GLOBAL VAR DECL: __exitval_ptr
	.extern .global .align 8 .u64 __exitval_ptr[1];
