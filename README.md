# Implementing a function wide defer in C

> [!CAUTION]
> This code was written for fun and should not be used in production. I just
> had a crazy idea and tried out to see if it was possible (and it is). There
> are compiler attributes (like `cleanup`) that can be used to achieve this in
> a better way; the goal here was to test an idea and learn, not to create a
> usable tool.

The idea lies behind adding the following assembly logic to the code:

- Allocate an 8 bytes long static variable:
  - 8 bytes will allow us to store an address.
  - It has to be static because we want it to be stored in the data section.
    The value stored here will be read after the `ret` of the current function
    has been executed. Before using `ret` the compiler will reset the base
    pointer, preventing us to access local memory.
- Save current return address (`[rbp + 8]`) in the static variable.
- Replace the return address with the `defer_begin` (new label; it is possible
  to generate multiple unique names using `__LINE__` in a macro).
- Jump to `defer_end` (skip deferred code).
- `defer_begin` (we arrive here after `ret` is executed):
  - Execute deferred code block.
  - Retrieve the saved return address from the data section.
  - Jump to the saved return address.
- `defer_end:`
  - Rest of the code.

This defer implementation is function wide and not scope wide because we rely
on the `ret` instruction to do the jump. Maybe there is a way to predict which
label GCC is generating for statements and write a hack using this, but this is
a much more complicated problem.

I used Gemini to generate the code in `main.c` based on this idea (it is pretty
simple, but C inline assembly syntax is quite awful). It works with GCC on
`x86-64`. The inline assembly code is tailored for GAS and uses the intel
syntax (because it is objectively better than AT&T ;D). It was tested on linux,
but it should work on windows as well (assuming the assembly is adapted for
msvc) since the C calling convention for functions is standardized.

It only works without compiler optimizations. At `O2`, the new code get's
optimized out (at least on my small test), but inlining and other optimization
tricks would also break the execution order of the defers, or even cause
crashes in some cases (the compiler being more clever but less rigorous when
using the stack and jumping around).

## More assembly background

In `x86` (and `x86-64`) assembly, calling a function is done using the `call`
instruction:

```asm
...              ; setup function arguments
call foo_address ; call the function
```

The `call` instruction will push the address of the next instruction (`rip`) on
the stack, and jump on at `foo_address` (which is the address of the first
instruction in the function foo).

The compiled functions have the following shape in assembly:

```asm
foo_address:
  push rbp      ; save the base pointer (begining of the stack of the caller)
  mov rbp, rsp  ; set the new frame (the stack of the foo starts here)
  ...           ; push the function arguments on the stack
  ...           ; allocate local variables
  ...           ; code (body of the function)
  mov rsp, rbp  ; restore the frame of the caller (value of `rsp` before `call`)
  pop rbp       ; restore the bottom of the frame of the caller
  ret           ; pop the return address pushed by `call`, and jump back to it
```

When a function ends, it restores the frame of the caller, pops the return
address pushed by `call`, and jumps to this address; going right after the `call`
instruction.

```asm
call foo_address
...              ; <- after `ret`, you go here
```

Knowing that `ret` pops the return address and jumps to it, we can reroute the
jump to `defer_begin` instead of the address after `call`. When the defer ends,
we jump back to the real return address that we saved earlier. When there are
multiple defers, they all save the current return address and override it so
they are all executed in the correct order when the function returns.
