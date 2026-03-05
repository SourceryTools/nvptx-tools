//     $ [nvptx-none-g++] -fno-builtin-memset 1-1.c -o 1-1-C++.s -S
// ..., and then manually simplify.

// BEGIN PREAMBLE
	.version	3.1
	.target	sm_35
	.address_size 64
// END PREAMBLE


// BEGIN FUNCTION DECL: _ZL15static_functionv
.func (.param.u32 %value_out) _ZL15static_functionv;

// BEGIN FUNCTION DEF: _ZL15static_functionv
.func (.param.u32 %value_out) _ZL15static_functionv
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: _Z15global_functioni
.visible .func (.param.u32 %value_out) _Z15global_functioni (.param.u32 %in_ar0);

// BEGIN GLOBAL FUNCTION DEF: _Z15global_functioni
.visible .func (.param.u32 %value_out) _Z15global_functioni (.param.u32 %in_ar0)
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: _Z16global_function2v
.visible .func (.param.u32 %value_out) _Z16global_function2v;

// BEGIN GLOBAL FUNCTION DEF: _Z16global_function2v
.visible .func (.param.u32 %value_out) _Z16global_function2v
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

// BEGIN GLOBAL VAR DECL: environ
	.extern .global .align 8 .u64 environ[1];


// BEGIN VAR DEF: _ZZ15global_functioniE21local_static_var_init
	.global .align 4 .u32 _ZZ15global_functioniE21local_static_var_init[1] = { 5 };

// BEGIN VAR DEF: _ZZ15global_functioniE16local_static_var
	.global .align 4 .u32 _ZZ15global_functioniE16local_static_var[1];

// BEGIN VAR DEF: _ZZL15static_functionvE21local_static_var_init
	.global .align 4 .u32 _ZZL15static_functionvE21local_static_var_init[1] = { 5 };

// BEGIN VAR DEF: _ZZL15static_functionvE16local_static_var
	.global .align 4 .u32 _ZZL15static_functionvE16local_static_var[1];

// BEGIN GLOBAL VAR DEF: global_var_init
	.visible .global .align 4 .u32 global_var_init[1] = { 26 };

// BEGIN GLOBAL VAR DEF: global_var
	.visible .global .align 4 .u32 global_var[1];

// BEGIN VAR DEF: _ZL15static_var_init
	.global .align 4 .u32 _ZL15static_var_init[1] = { 25 };

// BEGIN VAR DEF: _ZL10static_var
	.global .align 4 .u32 _ZL10static_var[1];

// BEGIN GLOBAL FUNCTION DECL: memset
.extern .func (.param.u64 %value_out) memset (.param.u64 %in_ar0, .param.u32 %in_ar1, .param.u64 %in_ar2);
