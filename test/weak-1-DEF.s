// BEGIN PREAMBLE
	.version	3.1
	.target	sm_35
	.address_size 64
// END PREAMBLE

// BEGIN GLOBAL FUNCTION DECL: __gbl_ctors
.visible .func __gbl_ctors;

// BEGIN GLOBAL FUNCTION DEF: __gbl_ctors
.visible .func __gbl_ctors
{
	// [...]
	ret;
}

// BEGIN GLOBAL VAR DEF: __exitval_ptr
	.visible .global .align 8 .u64 __exitval_ptr[1];
