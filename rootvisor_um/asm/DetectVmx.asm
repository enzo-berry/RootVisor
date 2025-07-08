.code
DetectVmxSupport PROC PUBLIC
    MOV    EAX, 1
    XOR    ECX, ECX
    CPUID
    
    ; Test bit 5 of ECX (VMX support bit)
    BT     ECX, 5
    JC     VMXSupported
    
    XOR    RAX, RAX
    JMP    EndFunction

VMXSupported:
    MOV    RAX, 1

EndFunction:
    RET

DetectVmxSupport ENDP

END