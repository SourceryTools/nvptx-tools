//     $ [nvptx-none-gcc] -fno-builtin-memset 1-1.c -o 1-1-C.s -S
// ..., and then manually simplify.

// BEGIN PREAMBLE
	.version	3.1
	.target	sm_35
	.address_size 64
// END PREAMBLE


// BEGIN FUNCTION DECL: static_function
.func (.param.u32 %value_out) static_function;

// BEGIN FUNCTION DEF: static_function
.func (.param.u32 %value_out) static_function
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: global_function
.visible .func (.param.u32 %value_out) global_function (.param.u32 %in_ar0);

// BEGIN GLOBAL FUNCTION DEF: global_function
.visible .func (.param.u32 %value_out) global_function (.param.u32 %in_ar0)
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: global_function2
.visible .func (.param.u32 %value_out) global_function2;

// BEGIN GLOBAL FUNCTION DEF: global_function2
.visible .func (.param.u32 %value_out) global_function2
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: non_mangled_function
.visible .func non_mangled_function;

// BEGIN GLOBAL FUNCTION DEF: non_mangled_function
.visible .func non_mangled_function
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: main
.visible .func (.param.u32 %value_out) main (.param.u32 %in_ar0, .param.u64 %in_ar1);

// BEGIN GLOBAL FUNCTION DEF: main
.visible .func (.param.u32 %value_out) main (.param.u32 %in_ar0, .param.u64 %in_ar1)
{
	// [...]
}


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

// BEGIN GLOBAL VAR DEF: global_var_init
	.visible .global .align 4 .u32 global_var_init[1] = { 26 };

// BEGIN GLOBAL VAR DEF: global_var
	.visible .global .align 4 .u32 global_var[1];

// BEGIN VAR DEF: static_var_init
	.global .align 4 .u32 static_var_init[1] = { 25 };

// BEGIN VAR DEF: static_var
	.global .align 4 .u32 static_var[1];

// BEGIN GLOBAL FUNCTION DECL: memset
.extern .func (.param.u64 %value_out) memset (.param.u64 %in_ar0, .param.u32 %in_ar1, .param.u64 %in_ar2);
