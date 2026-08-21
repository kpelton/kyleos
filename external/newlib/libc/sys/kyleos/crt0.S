
.global _start
.extern main
.type _start STT_FUNC
_start:
call main
.blah:
jmp .blah
