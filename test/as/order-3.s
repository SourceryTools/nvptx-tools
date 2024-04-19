.version 4.0
.target	sm_50
.address_size 64

//:FUNC_MAP "f1.1"
// comment 1
//:FUNC_MAP "f1.2"
//:IND_FUNC_MAP "if1.2"
//:VAR_MAP "v1"
.visible .entry e1 ()
{
	// [...]
}

// comment 2.1
//:FUNC_MAP "f2"
// comment 2.2
//:VAR_MAP "v2"
// comment 2.3
//:IND_FUNC_MAP "if2"
.visible .entry e2 ()
{
	// [...]
}

// The following form is never emitted by GCC, but still happens to be handled here ("by accident"):
/*:FUNC_MAP "f3" */
.visible .entry e3 ()
{
	// [...]
}

// The following form is never emitted by GCC, but still happens to be handled here ("by accident"):
/*:FUNC_MAP "f4"
:IND_FUNC_MAP "if4"
:VAR_MAP "v4" */
.visible .entry e4 ()
{
	// [...]
}

//:LABEL_MAP "l5"
.visible .entry e5 ()
{
	// [...]
}

//:IND_FUNC_MAP "if6"
.visible .entry e6 ()
{
	// [...]
}

//:IND_FUNC_MAP "if7"
