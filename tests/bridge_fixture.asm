option casemap:none
EXTERN CollectorStatuesDeath:PROC
PUBLIC CollectorStatuesDropEntry,CollectorStatuesDropReturn,BridgeCapture,ExpectedFlags
.data
CollectorStatuesDropEntry dq 0
CollectorStatuesDropReturn dq 0
BridgeCapture dq 28 dup (0)
ExpectedFlags dq 0
.code
RunDeathBridge PROC
    push rbx
    sub rsp,60h
    lea rax,OriginalDrop
    mov CollectorStatuesDropEntry,rax
    lea rax,Resumed
    mov CollectorStatuesDropReturn,rax
    mov QWORD PTR [rsp+20h],5
    mov QWORD PTR [rsp+28h],6
    mov QWORD PTR [rsp+30h],7
    mov QWORD PTR [rsp+38h],8
    mov QWORD PTR [rsp+40h],9
    mov QWORD PTR [rsp+48h],10
    mov rcx,11111111h
    mov rdx,22222222h
    mov r8,33333333h
    mov r9,44444444h
    mov r10,55555555h
    mov r11,66666666h
    mov rax,123456789abcdefh
    pcmpeqb xmm0,xmm0
    movdqa xmm1,xmm0
    movdqa xmm2,xmm0
    movdqa xmm3,xmm0
    movdqa xmm4,xmm0
    movdqa xmm5,xmm0
    cmp rax,1
    pushfq
    pop QWORD PTR [ExpectedFlags]
    jmp CollectorStatuesDeath
Resumed:
    add rsp,60h
    pop rbx
    ret
RunDeathBridge ENDP

; Deliberately destroy every volatile register in the observer to prove the
; production adapter restores every original native argument and live value.
CollectorStatuesObserveDeath PROC
    xor rax,rax
    xor rcx,rcx
    xor rdx,rdx
    xor r8,r8
    xor r9,r9
    xor r10,r10
    xor r11,r11
    pxor xmm0,xmm0
    pxor xmm1,xmm1
    pxor xmm2,xmm2
    pxor xmm3,xmm3
    pxor xmm4,xmm4
    pxor xmm5,xmm5
    ret
CollectorStatuesObserveDeath ENDP
OriginalDrop PROC
    mov [BridgeCapture],rax
    mov [BridgeCapture+8],rcx
    mov [BridgeCapture+16],rdx
    mov [BridgeCapture+24],r8
    mov [BridgeCapture+32],r9
    mov [BridgeCapture+40],r10
    mov [BridgeCapture+48],r11
    mov rax,[rsp]
    mov [BridgeCapture+56],rax
    pushfq
    pop QWORD PTR [BridgeCapture+64]
    mov rax,[rsp+28h]
    mov [BridgeCapture+72],rax
    mov rax,[rsp+30h]
    mov [BridgeCapture+80],rax
    mov rax,[rsp+38h]
    mov [BridgeCapture+88],rax
    mov rax,[rsp+40h]
    mov [BridgeCapture+96],rax
    mov rax,[rsp+48h]
    mov [BridgeCapture+104],rax
    mov rax,[rsp+50h]
    mov [BridgeCapture+112],rax
    movdqu XMMWORD PTR [BridgeCapture+128],xmm0
    movdqu XMMWORD PTR [BridgeCapture+144],xmm1
    movdqu XMMWORD PTR [BridgeCapture+160],xmm2
    movdqu XMMWORD PTR [BridgeCapture+176],xmm3
    movdqu XMMWORD PTR [BridgeCapture+192],xmm4
    movdqu XMMWORD PTR [BridgeCapture+208],xmm5
    ret
OriginalDrop ENDP
END
