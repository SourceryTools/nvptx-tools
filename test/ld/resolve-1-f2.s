// BEGIN PREAMBLE
.version 3.1
.target sm_30
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
