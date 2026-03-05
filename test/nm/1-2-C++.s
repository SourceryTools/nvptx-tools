//     $ [nvptx-none-g++] -fno-builtin-memset 1-2.c -o 1-2-C++.s -S
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

// BEGIN GLOBAL FUNCTION DECL: _Z15GLOBAL_FUNCTIONi
.visible .func (.param.u32 %value_out) _Z15GLOBAL_FUNCTIONi (.param.u32 %in_ar0);

// BEGIN GLOBAL FUNCTION DEF: _Z15GLOBAL_FUNCTIONi
.visible .func (.param.u32 %value_out) _Z15GLOBAL_FUNCTIONi (.param.u32 %in_ar0)
{
	// [...]
}

// BEGIN GLOBAL FUNCTION DECL: _Z16GLOBAL_FUNCTION2v
.visible .func (.param.u32 %value_out) _Z16GLOBAL_FUNCTION2v;

// BEGIN GLOBAL FUNCTION DEF: _Z16GLOBAL_FUNCTION2v
.visible .func (.param.u32 %value_out) _Z16GLOBAL_FUNCTION2v
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

// BEGIN GLOBAL FUNCTION DECL: _Z4MAINv
.visible .func (.param.u32 %value_out) _Z4MAINv;

// BEGIN GLOBAL FUNCTION DEF: _Z4MAINv
.visible .func (.param.u32 %value_out) _Z4MAINv
{
	// [...]
}

// BEGIN GLOBAL VAR DECL: global_var
	.extern .global .align 4 .u32 global_var[1];

// BEGIN GLOBAL VAR DECL: environ
	.extern .global .align 8 .u64 environ[1];


// BEGIN VAR DEF: _ZZ15GLOBAL_FUNCTIONiE21local_static_var_init
	.global .align 4 .u32 _ZZ15GLOBAL_FUNCTIONiE21local_static_var_init[1] = { 5 };

// BEGIN VAR DEF: _ZZ15GLOBAL_FUNCTIONiE16local_static_var
	.global .align 4 .u32 _ZZ15GLOBAL_FUNCTIONiE16local_static_var[1];

// BEGIN VAR DEF: _ZZL15static_functionvE21local_static_var_init
	.global .align 4 .u32 _ZZL15static_functionvE21local_static_var_init[1] = { 5 };

// BEGIN VAR DEF: _ZZL15static_functionvE16local_static_var
	.global .align 4 .u32 _ZZL15static_functionvE16local_static_var[1];

// BEGIN VAR DEF: _ZL15static_var_init
	.global .align 4 .u32 _ZL15static_var_init[1] = { 25 };

// BEGIN VAR DEF: _ZL10static_var
	.global .align 4 .u32 _ZL10static_var[1];

// BEGIN GLOBAL FUNCTION DECL: memset
.extern .func (.param.u64 %value_out) memset (.param.u64 %in_ar0, .param.u32 %in_ar1, .param.u64 %in_ar2);
