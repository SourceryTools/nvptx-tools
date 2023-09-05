// Based on offloading-compilation artifact 'a.xnvptx-none.mkoffload.s' of <https://gcc.gnu.org/PR100059> "[OpenMP] wrong code with 'declare target link' and a scalar variable".

// BEGIN PREAMBLE
	.version 4.0
	.target	sm_50
	.address_size 64
// END PREAMBLE


// BEGIN GLOBAL FUNCTION DECL: update
.visible .func update;

// BEGIN GLOBAL FUNCTION DEF: update
.visible .func update
{
	// [...]
}
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
//:FUNC_MAP "main$_omp_fn$0"
//:VAR_MAP "i$linkptr"
//:VAR_MAP "b$linkptr"
//:VAR_MAP "c$linkptr"
//:VAR_MAP "a$linkptr"


// BEGIN GLOBAL VAR DEF: i$linkptr
	.visible .global .align 8 .u64 i$linkptr[1];

// BEGIN GLOBAL VAR DEF: b$linkptr
	.visible .global .align 8 .u64 b$linkptr[1];

// BEGIN GLOBAL VAR DEF: c$linkptr
	.visible .global .align 8 .u64 c$linkptr[1];

// BEGIN GLOBAL VAR DEF: a$linkptr
	.visible .global .align 8 .u64 a$linkptr[1];

// BEGIN GLOBAL FUNCTION DECL: gomp_nvptx_main
.extern .func gomp_nvptx_main (.param.u64 %in_ar1, .param.u64 %in_ar2);

// BEGIN GLOBAL VAR DECL: __nvptx_stacks
.extern .shared .u64 __nvptx_stacks[32];

// BEGIN GLOBAL VAR DECL: __nvptx_uni
.extern .shared .u32 __nvptx_uni[32];
