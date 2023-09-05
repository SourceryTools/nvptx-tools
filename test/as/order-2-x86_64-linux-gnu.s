// Based on offloading-compilation artifact 'a.xnvptx-none.mkoffload.s' of GCC 'libgomp.c-c++-common/declare_target-1.c'.

// BEGIN PREAMBLE
	.version 4.0
	.target sm_50
	.address_size 64
// END PREAMBLE

.visible .entry main$_omp_fn$0 (.param.u64 %arg, .param.u64 %stack, .param.u64 %sz)
{
	// [...]
}

// BEGIN FUNCTION DECL: main$_omp_fn$0$impl
.func main$_omp_fn$0$impl (.param.u64 %in_ar0);

// BEGIN FUNCTION DEF: main$_omp_fn$0$impl
.func main$_omp_fn$0$impl (.param.u64 %in_ar0)
{
	// [...]
}


// BEGIN GLOBAL VAR DEF: data
	.visible .global .align 4 .u32 data[1] = { 5 };
//:FUNC_MAP "main$_omp_fn$0"
//:VAR_MAP "data"

// BEGIN GLOBAL FUNCTION DECL: gomp_nvptx_main
.extern .func gomp_nvptx_main (.param.u64 %in_ar1, .param.u64 %in_ar2);

// BEGIN GLOBAL VAR DECL: __nvptx_stacks
.extern .shared .u64 __nvptx_stacks[32];

// BEGIN GLOBAL VAR DECL: __nvptx_uni
.extern .shared .u32 __nvptx_uni[32];
