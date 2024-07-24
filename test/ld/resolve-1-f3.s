// BEGIN PREAMBLE
.version 4.0
.target sm_50
.address_size 64
// END PREAMBLE

// BEGIN GLOBAL FUNCTION DECL: f1
.extern .func f1;

// BEGIN GLOBAL FUNCTION DECL: f2
.extern .func f2;

// BEGIN GLOBAL FUNCTION DECL: f3
.visible .func f3;

// BEGIN GLOBAL FUNCTION DEF: f3
.visible .func f3
{
	// [...]
	ret;
}
