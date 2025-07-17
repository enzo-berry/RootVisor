.code
GetCpuVendorAsm PROC PUBLIC
    push rbx
    push rdx
    push r8
    
    mov r8, rcx
    
    xor eax, eax        ; EAX = 0
    cpuid               ; Execute CPUID instruction
                        ; CPUID destroys EAX, EBX, ECX, EDX
    mov [r8], ebx
    mov [r8+4], edx
    mov [r8+8], ecx
    mov byte ptr [r8+12], 0


    pop r8
    pop rdx
    pop rbx
    
    ret
GetCpuVendorAsm ENDP
END