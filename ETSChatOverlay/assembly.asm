option casemap :none

_TEXT    SEGMENT
    ;https://msdn.microsoft.com/en-us/library/windows/hardware/ff561499(v=vs.85).aspx

    ;mangled functions
    EXTERN dxHookFunc:               PROC;    EngineHook::_scriptEntered

    ;JmpBacks

    EXTERN presentHookJmpBack:                                  qword
    EXTERN endSceneHookJmpBack:                                  qword
    
    ;misc
    EXTERN dxDevice:                                    qword

    PUBLIC presentHook
    presentHook PROC

        xor         r9d, r9d
        xor         r8d, r8d
        mov         qword ptr[rsp + 20h], r14
        xor         edx, edx

        push        rcx
        push        rdx
        ;push        r8
        ;push        r9
        ;push        r10
        ;push        r11

        mov         rax, offset dxDevice;

        mov         [rax], rcx;
        mov         rax, qword ptr[rcx]


        call        dxHookFunc;

        ;pop        r11
        ;pop        r10
        ;pop        r9
        ;pop        r8
        pop        rdx
        pop        rcx

        mov         rax, qword ptr[rcx]

        call        qword ptr[rax + 88h]



        jmp         presentHookJmpBack;

    presentHook ENDP


    PUBLIC endSceneHook
    endSceneHook PROC

        sub     rsp, 28h
        mov     rcx, [rcx+118h]

        push        rcx
        push        rdx
        ;push        r8
        ;push        r9
        ;push        r10
        ;push        r11

        mov         rax, offset dxDevice;

        mov         [rax], rcx;

        call        dxHookFunc;

        ;pop        r11
        ;pop        r10
        ;pop        r9
        ;pop        r8
        pop        rdx
        pop        rcx

        mov         rax, [rcx]
        call        qword ptr [rax+150h]


        jmp         endSceneHookJmpBack;

    endSceneHook ENDP


_TEXT    ENDS
END