// BEGIN PREAMBLE
.version 4.0
.target sm_50
.address_size 64
// END PREAMBLE

// BEGIN GLOBAL FUNCTION DECL: f1
.extern .func f1;

// BEGIN GLOBAL FUNCTION DECL: f2
.visible .func f2;

// BEGIN GLOBAL FUNCTION DEF: f2
.visible .func f2
{
	// [...]
	ret;
}

// BEGIN GLOBAL FUNCTION DECL: f3
.extern .func f3;
