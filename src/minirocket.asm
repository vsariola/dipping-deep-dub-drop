bits 32

%include "minirocket.inc"

section		.rocket     code    align=1
global  _minirocket_sync@8
_minirocket_sync@8:
    pushad
    xor     ecx, ecx
    mov     ebx, row_data
    mov     edi, start_times
    mov     esi, dword [esp+40] ; esi = outptr
    fld     dword [esp+36]      ; t
    jmp     .writesync
.nextkey:
    fstp    st0
    fstp    st0
    movzx   edx, word [ebx+edx*2]   ; ebx = d
    add     dword [edi], edx  ; t0 += d
    inc     word [ecx*2+track_data+ebx-row_data]    ; index++
.checkkey:
    movzx   edx, word [ecx*2+track_data+ebx-row_data]
    fld     dword [esp+36]      ; t
    fisub   dword [edi]   ; t-t0
    fild    word [ebx+edx*2]    ; d t-t0
    fcomi   st1                 ; if (d >= t-t0)
    jb     .nextkey             ;   goto .key;
.key:
    fdivp   st1, st0            ; a=(t-t0)/d
    test    byte [value_data+edx*2+ebx-row_data], 1
    jnz		.out
    fstp    st0     ; 
    fldz            ; 0
.out:
    fild    word [value_data+edx*2+ebx-row_data]    ; v0*256 a
    fidiv   word [c_256]           ; v0 a   % WARNING: we assume row data starts with 0x00 0x01 aka word 256... this is not universally true but for this intro it is
    fild    word [value_data+edx*2+2+ebx-row_data]  ; v1*256 v0 a
    fidiv   word [c_256]           ; v1 v0 a
    fsub    st0, st1    ; v1-v0 v0 a
    fmulp   st2         ; v0 a*(v1-v0)
    faddp   st1         ; v0+a*(v1-v0)   
    inc     ecx
    add     edi, 4
.writesync:
    fstp	dword [esi]
    lodsd
    cmp     cl, numtracks
    jl      .checkkey
    popad
    ret     8

c_256:
    dw 0x100

section		.rtbss      bss		align=1
start_times resd    numtracks

