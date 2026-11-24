option casemap:none
EXTERN CollectorStatuesObserveDeath:PROC
EXTERN CollectorStatuesDropEntry:QWORD
EXTERN CollectorStatuesDropReturn:QWORD
.code
; Entered by JMP at the original CALL. RSP is 16-byte aligned. Preserve
; all input registers, flags, XMM0..5 and all original stack arguments.
CollectorStatuesDeath PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp,80h
    movdqu [rsp+20h],xmm0
    movdqu [rsp+30h],xmm1
    movdqu [rsp+40h],xmm2
    movdqu [rsp+50h],xmm3
    movdqu [rsp+60h],xmm4
    movdqu [rsp+70h],xmm5
    call CollectorStatuesObserveDeath
    movdqu xmm0,[rsp+20h]
    movdqu xmm1,[rsp+30h]
    movdqu xmm2,[rsp+40h]
    movdqu xmm3,[rsp+50h]
    movdqu xmm4,[rsp+60h]
    movdqu xmm5,[rsp+70h]
    add rsp,80h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    ; Reproduce CALL's exact return address. The Descent checks this address
    ; to distinguish monster deaths from its own additional treasure picks.
    push QWORD PTR [CollectorStatuesDropReturn]
    jmp QWORD PTR [CollectorStatuesDropEntry]
CollectorStatuesDeath ENDP
END
