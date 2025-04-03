.386
.model flat, stdcall
.stack 4096
ExitProcess PROTO, dwExitCode:DWORD

include Irvine32.inc

.data
    message db "Hello, world!", 0

.code
main PROC
    ; Get a handle to the console output
    push -11
    call GetStdHandle

    ; Write the message to the console
    push 0
    lea edx, message
    push edx
    push 13 ; Message length
    push eax
    call WriteConsoleA

    ; Exit program
    push 0
    call ExitProcess
main ENDP

END main
