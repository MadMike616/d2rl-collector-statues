; The guarded original CALL sites already provide the 32-byte shadow space
; and pre-CALL alignment. Preserve that ABI and resume at the next instruction.
EXTERN CollectorStatuesLookup:PROC
EXTERN CollectorStatuesContinueA:QWORD
EXTERN CollectorStatuesContinueB:QWORD
.code
CollectorStatuesTooltipA PROC
    mov rdx, rbx
    call CollectorStatuesLookup
    jmp QWORD PTR [CollectorStatuesContinueA]
CollectorStatuesTooltipA ENDP
CollectorStatuesTooltipB PROC
    mov rdx, rsi
    call CollectorStatuesLookup
    jmp QWORD PTR [CollectorStatuesContinueB]
CollectorStatuesTooltipB ENDP
END
