BITS 64

SECTION .text
global gdt_load

gdt_load:
    lgdt [rdi]

    push qword 0x08
    lea rax, [rel .reload_segments]
    push rax
    retfq

.reload_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax
    ret
