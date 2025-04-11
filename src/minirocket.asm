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
    
    movzx   eax, byte [type_data+edx+ebx-row_data]
  
     dec		eax
 	 jz		.done  
     dec		eax 	 
     jnz		.key_ramp
     fild    word [c_three] ; 3 a
     fsub    st0, st1                    ; 3-a a
     fsub    st0, st1                    ; 3-2*a a
     fmul    st0, st1                    ; a*(3-2*a) a
     fmulp   st1                         ; a*a*(3-2*a)
     jmp .done
 .key_ramp:
     dec		eax
 	 jnz		.key_step     
     fmul    st0     ; a^2
     jmp .done
 .key_step:
     fldz            ; 0 a
     fstp    st1     ; a
 .done:

    fild    word [value_data+edx*2+ebx-row_data]    ; v0*256 a
    fidiv   word [c_256]           ; v0 a   
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

c_three:
    dw 3

section		.rtbss      bss		align=1
start_times resd    numtracks

