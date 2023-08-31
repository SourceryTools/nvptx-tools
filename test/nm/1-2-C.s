//     $ [nvptx-none-gcc] -fno-builtin-memset 1-2.c -o 1-2-C.s -S
// ..., and then manually simplify.

// BEGIN PREAMBLE
	.version 4.0
	.target	sm_50
	.address_size 64
// END PREAMBLE


// BEGIN FUNCTION DECL: static_function
.func (.param.u32 %value_out) static_function;

// BEGIN FUNCTION DEF: static_function
.func (.param.u32 %value_out) static_function
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: GLOBAL_FUNCTION
.visible .func (.param.u32 %value_out) GLOBAL_FUNCTION (.param.u32 %in_ar0);

// BEGIN GLOBAL FUNCTION DEF: GLOBAL_FUNCTION
.visible .func (.param.u32 %value_out) GLOBAL_FUNCTION (.param.u32 %in_ar0)
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: GLOBAL_FUNCTION2
.visible .func (.param.u32 %value_out) GLOBAL_FUNCTION2;

// BEGIN GLOBAL FUNCTION DEF: GLOBAL_FUNCTION2
.visible .func (.param.u32 %value_out) GLOBAL_FUNCTION2
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: NON_MANGLED_FUNCTION
.visible .func NON_MANGLED_FUNCTION;

// BEGIN GLOBAL FUNCTION DEF: NON_MANGLED_FUNCTION
.visible .func NON_MANGLED_FUNCTION
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: MAIN
.visible .func (.param.u32 %value_out) MAIN;

// BEGIN GLOBAL FUNCTION DEF: MAIN
.visible .func (.param.u32 %value_out) MAIN
{
	// [...]
}

// BEGIN GLOBAL VAR DECL: global_var
	.extern .global .align 4 .u32 global_var[1];


// BEGIN VAR DEF: local_static_var_init$0
	.global .align 4 .u32 local_static_var_init$0[1] = { 5 };

// BEGIN VAR DEF: local_static_var$1
	.global .align 4 .u32 local_static_var$1[1];

// BEGIN VAR DEF: local_static_var_init$2
	.global .align 4 .u32 local_static_var_init$2[1] = { 5 };

// BEGIN VAR DEF: local_static_var$3
	.global .align 4 .u32 local_static_var$3[1];

// BEGIN GLOBAL VAR DECL: environ
	.extern .global .align 8 .u64 environ[1];

// BEGIN VAR DEF: static_var_init
	.global .align 4 .u32 static_var_init[1] = { 25 };

// BEGIN VAR DEF: static_var
	.global .align 4 .u32 static_var[1];

// BEGIN GLOBAL FUNCTION DECL: memset
.extern .func (.param.u64 %value_out) memset (.param.u64 %in_ar0, .param.u32 %in_ar1, .param.u64 %in_ar2);
