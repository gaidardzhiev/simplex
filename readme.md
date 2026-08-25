# Simplex

Simplex is a compiled systems language for AArch64 Linux, without stdlib, linker, or runtime. The compiler reads source code and spits out a self contained ELF64 executable directly, it targets AArch64 and produces correct position dependent executables with no external dependencies at any stage. The end goal is for the compiler to compile a complete, reproducible version of itself, not only as a demonstration of cleverness but as a proof that the language and its code generator are correct, expressive, and complete enough to be trusted as a system tool. Self compilation is the only test that cannot be faked.

## The language

One type: `int`. A variable holding a string literal holds its address, pointer variables hold addresses of other variables, the compiler knows the difference.

[fact.x](src/fact.x):
```c
int fact(int n) {
	if (n == 0) { return 1; }
	return n * fact(n - 1);
}

putint(fact(10));
putstr("\n");
```

[ptr.x](src/ptr.x):
```c
int x = 99;
int *p = &x;
putint(*p);
putstr("\n");
*p = 42;
putint(x);
putstr("\n");
```

Compile the compiler:
```sh
make
/usr/bin/musl-gcc -o simplex2elf simplex2elf.c -static -no-pie -g -Wall -Wextra
```

Compile [hello.x](src/hello.x) to `fact.out`:
```sh
./simplex2elf src/hello.x -o hello.out
wrote AArch64 ELF64 to src/hello.out (4127 bytes, entry 0x4001d0)
```

Execute [hello.out](src/hello.out)
```sh
src/hello.out
Hello from the Simplex World!
```

Keywords: `int`, `if`, `else`, `while`, `return`. Everything else is a function call or an operator. `putint`, `putstr`, `getc`, `bload`, `bstore`, and `balloc` are built in intrinsics. There is no standard library. There is no preprocessor. There is no separate compilation.

Operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`, unary `-`, `&` (address-of), `*` (dereference), `[]` (index).

Pointer operations:

```c
int *p = &x;     /* address of local or global */
*p               /* dereference: load through pointer */
*p = v;          /* store through pointer */
p[i]             /* index: equivalent to *(p + i*4) */
```

Byte and heap operations:

```c
bload(p, i)      /* load byte at p[i] */
bstore(p, i, v)  /* store byte v at p[i] */
balloc(n)        /* bump allocate n bytes, returns pointer, arena lifetime */
```

Conditions are integers, zero is false, anything else is true, exactly as in C. `else if` chains parse naturally, no special token needed. What is absent: structs, multiple types, `for`, `break`, `continue`, `switch`, type checking.

## The compiler

[simplex2elf.c](./simplex2elf.c) and [simplex2elf.h](./simplex2elf.h) compiled with any C99 compatible compiler and `make`.

The compiler is a recursive descent parser feeding directly into an AArch64 code emitter. There is no intermediate representation. No AST transformation passes. No optimizations. The parser produces an AST, the code generator walks it once and emits A64 instructions into a buffer, the buffer is written as an ELF64 file. Three segments: text at `0x400000`, read-only data at `0x500000`, BSS at `0x600000`. The entry point is recorded after the runtime helpers are emitted.

Forward calls including recursion are patched in a single pass at the end of codegen. All calling convention logic lives in three functions: `emit_fn_entry`, `emit_fn_exit`, `emit_call`. They do not duplicate each other.

The runtime provides three internal functions: `__itoa`, `__out`, `__putstr`. They are registered in the function table and called via `BL` like any other function.

AArch64 calling convention: up to 8 arguments in `X0`–`X7`. Return value in `X0`. Callee saves `X29` (frame pointer) and `X30` (link register). All temporary stack slots are 16 bytes to maintain the mandatory 16-byte SP alignment at every `BL`.

Syscall ABI (Linux AArch64): number in `X8`, arguments in `X0`-`X5`, `SVC #0`. write=64, read=63, exit=93.

## Self hosting

Self hosting means the compiler compiles its own source and produces a binary that can compile that same source again. The output must be bit-identical across generations. That is the only test that cannot be faked and the one this project is working toward.

The bootstrap is built in three stages inside the [stage1/](stage1/) directory. [lex.x](stage1/lex.x) implements the lexer: a single `lx_one` function that consumes one token at a time from a byte buffer, identifies keywords via `streqn` against null-terminated keyword strings, and returns a 20-byte token allocated from the bump arena. [parse.x](stage1/parse.x) contains the full lexer plus a recursive descent parser that produces a heap-allocated AST. Each node is a fixed 232-byte block with fields for type, a numeric value, a string pointer, three child slots, a variable-length child array capped at 16, and a parameter list capped at 16. No dynamic resizing. [codegen.x](stage1/codegen.x) will contain the AArch64 ELF64 emitter written in `x`, at which point the three files are concatenated with their test drivers stripped and a single entry point added at the bottom. The resulting [compiler.x](stage1/compiler.x) is compiled by the C host to produce `stage1/compiler.out`, which then compiles [compiler.x](stage1/compiler.x) itself. That is stage2. If stage2 output matches stage1 output byte for byte, self hosting is complete.

The self-hosting target is not distant. The language has pointers, array indexing, byte access, and a bump allocator. What remains is structs or a workable substitute to express the compiler's own data structures. When that exists, the compiler can be written in itself and the bootstrap chain becomes fully auditable from source to binary.

There is no linker and none is needed, because `Simplex` has no separate compilation stages. The top level of a source file is sequential: globals and functions declared earlier are visible to everything that follows. Concatenation in dependency order is the link step, so the only tool required is `cat`...

One constraint shaped the design of all [stage1/](stage1/) code and must be respected in any `Simplex` source: all local variables must be declared at the top of a function before the first `if` or `while`. The frame pointer fix (`MOV SP, X29` before `LDP` in `emit_fn_exit`) is already applied in this port, a return from any function correctly unwinds the frame regardless of where locals were declared.

## Status

- [hello.x](src/hello.x): print a string literal stored in a global variable
- [vars.x](src/vars.x): global variables and integer arithmetic
- [loop.x](src/loop.x): while loop, global state
- [fact.x](src/fact.x): recursive factorial, the correctness baseline
- [ptr.x](src/ptr.x): address of, dereference, store through pointer
- [arr.x](src/arr.x): array indexing through pointer, scaled by 4
- [bytes.x](src/bytes.x): byte level read via bload
- [alloc.x](src/alloc.x): bump allocator, 64KB arena in BSS
- [stage1/lex.x](stage1/lex.x): tokenizer, lx_* stream, strings, numbers, ids, operators
- [stage1/parse.x](stage1/parse.x): recursive descent parser, ND_* AST, functions and control flow

## License

This project is provided under the [GPL3 License](./COPYING) Copyright (C) 2026 Ivan Gaydardzhiev
