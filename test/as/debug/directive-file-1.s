//     $ [nvptx-none-gcc] directive-file-1.c -o directive-file-1.s -S -g
//     $ [nvptx-none-as] directive-file-1.s -o directive-file-1.o.golden


// BEGIN PREAMBLE
	.version 4.0
	.target	sm_50
	.address_size 64
// END PREAMBLE


// BEGIN GLOBAL FUNCTION DECL: f
.visible .func (.param.u32 %value_out) f (.param.u32 %in_ar0, .param.u32 %in_ar1);

// BEGIN GLOBAL FUNCTION DEF: f
.visible .func (.param.u32 %value_out) f (.param.u32 %in_ar0, .param.u32 %in_ar1)
{
	.reg.u32 %value;
	.reg.u32 %ar0;
	ld.param.u32 %ar0, [%in_ar0];
	.reg.u32 %ar1;
	ld.param.u32 %ar1, [%in_ar1];
	.local .align 16 .b8 %frame_ar[24];
	.reg.u64 %frame;
	cvta.local.u64 %frame, %frame_ar;
	.reg.u32 %r22;
	.reg.u32 %r23;
	.reg.u32 %r24;
	.reg.u32 %r25;
	.reg.u32 %r26;
	.reg.u32 %r27;
	.reg.u32 %r28;
	.reg.u32 %r29;
		mov.u32	%r24, %ar0;
		st.u32	[%frame+16], %r24;
		mov.u32	%r25, %ar1;
		st.u32	[%frame+20], %r25;
		ld.u32	%r26, [%frame+16];
		st.u32	[%frame], %r26;
		ld.u32	%r27, [%frame+20];
		st.u32	[%frame+4], %r27;
	.file 1 "directive-file-1-sum.c"
	.loc 1 4 12
		ld.u32	%r28, [%frame];
		ld.u32	%r29, [%frame+4];
		add.u32	%r22, %r28, %r29;
	.file 2 "directive-file-1.c"
	.loc 2 5 10
		mov.u32	%r23, %r22;
	.loc 2 6 1
		mov.u32	%value, %r23;
	st.param.u32	[%value_out], %value;
	ret;
}
