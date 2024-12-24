; add.asm

.code
add_numbers proc
  mov rax, rcx  ; Load the first argument into RAX
  add rax, rdx  ; Add the second argument
  ret
add_numbers endp
end
