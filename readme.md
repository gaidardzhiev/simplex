# Simplex

Simplex is a compiled systems language for AArch64 Linux. Its compiler reads source code and spits out self contained ELF64 executables directly. The output is correct, position dependent executables for AArch64, and no external dependencies are needed at any stage. The compiler compiles a complete, reproducible version of itself, not only as a demonstration of cleverness but as a proof that the language and its code generator are correct, expressive, and complete enough to be trusted as a system tool. Self compilation is the only test that cannot be faked!

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

Compile [hello.x](src/hello.x) to `hello.out`:
```sh
./simplex2elf src/hello.x -o hello.out
wrote AArch64 ELF64 to src/hello.out (4127 bytes, entry 0x4001d0)
```

Execute [hello.out](src/hello.out)
```sh
src/hello.out
Hello from the Simplex World!
```

Keywords: `int`, `if`, `else`, `while`, `return`. Everything else is a function call or an operator. `putint`, `putstr`, `getc`, `bload`, `bstore`, and `balloc` are built in intrinsics.

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
bstore(v, p, i)  /* store byte v at p[i] */
balloc(n)        /* bump allocate n bytes, returns pointer, arena lifetime */
```

Conditions are integers, zero is false, anything else is true, exactly as in C. `else if` chains parse naturally, no special token needed. What is absent: structs, multiple types, `for`, `break`, `continue`, `switch`, type checking.

## The compiler

[simplex2elf.c](./simplex2elf.c) and [simplex2elf.h](./simplex2elf.h) compiled with any C99 compatible compiler and `make`.

The compiler is a recursive descent parser feeding directly into an AArch64 code emitter. There is no intermediate representation or AST transformation passes. No silly optimizations. The parser produces an AST, the code generator walks it once and emits A64 instructions into a buffer, the buffer is written as an ELF64 file. Three segments: text at `0x400000`, read-only data at `0x500000`, BSS at `0x600000`. The entry point is recorded after the runtime helpers are emitted.

Forward calls including recursion are patched in a single pass at the end of codegen. All calling convention logic lives in three functions: `emit_fn_entry`, `emit_fn_exit`, `emit_call`. They do not duplicate each other.

The runtime provides three internal functions: `__itoa`, `__out`, `__putstr`. They are registered in the function table and called via `BL` like any other function.

AArch64 calling convention: up to 8 arguments in `X0`–`X7`. Return value in `X0`. Callee saves `X29` (frame pointer) and `X30` (link register). All temporary stack slots are 16 bytes to maintain the mandatory 16-byte SP alignment at every `BL`.

Syscall ABI (Linux AArch64): number in `X8`, arguments in `X0`-`X5`, `SVC #0`. write=64, read=63, exit=93.

## Self hosting

Self hosting means the compiler compiles its own source and produces a binary that can compile that same source again, with bit-identical output across generations. That is the only test that cannot be faked, and it now passes: `stage1.out`, `stage2.out` and `stage3.out` share one SHA-256.

```sh
make selfhost
```

The compiler is written in `x` and lives in [stage1/](stage1/). [parse.x](stage1/parse.x) holds the lexer (a single `lx_one` that consumes one token at a time from a byte buffer and returns a 20-byte token from the bump arena) and a recursive descent parser producing a heap-allocated AST of fixed 232-byte nodes. [codegen.x](stage1/codegen.x) is a line-for-line port of the C code generator: the same AArch64 encoders, the same push/pop expression scheme, the same lazy value kinds (immediate, register, frame slot, global), the same forward-call patching, the same runtime helpers `__itoa`, `__out`, `__putstr` and the same ELF64 layout. [main.x](stage1/main.x) is the entry point: read source, parse, generate, emit. [lex.x](stage1/lex.x) is a standalone lexer test, since `parse.x` already contains the lexer.

The self-hosted compiler is [parse.x](stage1/parse.x) with its test driver and dump helpers stripped (everything from `int nd_type_name` onward), followed by [codegen.x](stage1/codegen.x) and [main.x](stage1/main.x). There is no linker. Concatenation in dependency order is the link step, so the only tool required is `cat`.

The bootstrap chain, as executed by `selfhost.sh`:

1. `simplex2elf` (C) compiles `simplex.x` into `stage1.out`.
2. `stage1.out` compiles `simplex.x` into `stage2.out`.
3. `stage2.out` compiles `simplex.x` into `stage3.out`.
4. `stage1.out`, `stage2.out` and `stage3.out` must be byte identical.
5. `stage2.out` must also produce the same bytes as `simplex2elf` for every program in `src/`, `stage1/lex.x`, `stage1/parse.x` and `tests/`.

### What had to change in the C compiler and the parser

Four obstacles stood between the language and its own compiler.

- **Heap.** The arena was 64 KB, which holds about 280 AST nodes. `HEAP_SZ` is now 64 MB. `MAX_FNS` was raised from 256 to 1024 and `MAX_FPATCHES` from 4096 to 32768, because the compiler's own source exceeds the old limits. These are the only edits to `simplex2elf.c`, and they change the BSS size recorded in every emitted binary. The same values sit at the top of `codegen.x` (`HEAP_SZ`, `MAX_FNS`, `MAX_FPAT`, `MAX_GLBS`) and must be kept in sync.
- **Children.** Children were stored in a fixed array inside the node and overran silently past about 49, while the top level of a compiler has hundreds. A node now holds a pointer to a growable array (`p[9]`) and its capacity (`p[10]`); `nd_ch` and `nd_add_ch` are the only functions that changed.
- **Output.** There is no raw byte output: `putstr` stops at the first NUL and an ELF is full of NULs. The compiler prints the ELF as `\ooo` octal escapes, four characters per byte, and `sx.sh` decodes that with POSIX `printf`.
- **Input.** `getc` never signals EOF, it keeps returning the last byte read. The compiler reads stdin until a NUL byte, which `sx.sh` appends. Without it the compiler stops with `source too large or missing NUL terminator`.

### Rules the source must obey

These are properties of the language that [codegen.x](stage1/codegen.x) and [main.x](stage1/main.x) were written around.

- A function has at most 16 variables, parameters and locals together. Frame slots are addressed with `stur`/`ldur`, whose offset is 9 bits signed, so slot 17 silently wraps. Neither compiler checks this.
- Values are unsigned 32 bit. There are no negative comparisons, `/` and `%` are unsigned, and results that go negative stay 64 bit until they are stored.
- `&&` and `||` are bitwise and do not short circuit, so they are only safe on `0` or `1`.
- There are no hex literals and no shifts, so instruction words are built with decimal constants and multiplication by powers of two.
- Locals are declared at the top of a function, before the first `if` or `while`.
- `bstore` takes `(value, ptr, idx)`.
- Errors print a message and crash deliberately, because there is no `exit` intrinsic. Output is written only after the whole compile succeeds, so a failed compile leaves no partial ELF.

### Scripts

[sx.sh](sx.sh) runs a Simplex compiler binary on a source file: `./sx.sh compiler src.x out`. It appends the NUL sentinel to the source, pipes it through the compiler into `out.txt`, decodes the escapes into the binary `out` with `printf "$(cat out.txt)"`, and sets the executable bit. The compiler's exit status is checked before decoding, so a crashed compile prints its message and leaves nothing behind. It works with either `stage1.out` or `stage2.out`.

[selfhost.sh](selfhost.sh) builds `simplex2elf` if needed, assembles `simplex.x`, runs the three stages, compares them with `cmp`, prints the hashes, and then checks stage 2 against the C compiler on every program in `src/`, `stage1/lex.x`, `stage1/parse.x` and `tests/`. Anything that is not a regular file is skipped. It exits non zero if any stage or program differs. `make selfhost` runs it and `make clean` removes its products.

### Tests

[tests/](tests/) holds programs written to reach code generator paths the compiler's own source does not use: every operator and precedence, unary minus and `!`, pointers, `&` on locals and globals, index and dereference stores, calls with 0 to 8 arguments, forward and mutual recursion, string escapes (`\n \t \\ \" \0` and unknown escapes), the layout with no read-only segment, large immediates, `getc`/`bstore`/`balloc`, a function with exactly 16 slots, discarded expression statements, comments, an empty program and a top level `return`. The check is byte equality against the C compiler, not just matching output.

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
- [stage1/codegen.x](stage1/codegen.x): AArch64 code generator and ELF64 emitter in `x`
- [stage1/main.x](stage1/main.x): self hosted compiler entry point
- [sx.sh](sx.sh), [selfhost.sh](selfhost.sh): run a compiler binary on a source, run the three stage bootstrap
- [tests/](tests/): code generator coverage, compared byte for byte with the C compiler
- [simplex.x](simplex.x): the self hosted compiler, `stage1/parse.x` + `stage1/codegen.x` + `stage1/main.x` concatenated, compiles itself to a byte identical fixpoint

## License

This project is provided under the [GPL3 License](./COPYING) Copyright (C) 2026 Ivan Gaydardzhiev
