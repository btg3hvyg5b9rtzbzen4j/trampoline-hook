# Simple Trampoline Hookcode
> Other Tags: Counter-Strike Source, EndScene Hook, Direct3D9, D3D9 

This is a simple (internal) trampoline hook written in C. The purpose of this project/repository is to help people understand trampoline hooks better. I achieve that effect (for myself, at least) by doing everything as minimally and as simply as possible.

## What is a trampoline hook?
A trampoline hook is a technique for intercepting function calls at runtime without writing inline assembly code. Traditional detours require writing inline assembly instructions (the stolen bytes) at the end of your hook, and maybe even fixing up the stack. A trampoline hook wraps this process in a more dynamic approach, allowing the code to stay cleaner and allowing manual assembly work to be done automatically at runtime. In simplified terms, it works by dynamically "creating" a function at runtime that will execute the stolen bytes and jump back to the original function to restore flow.

### Important differentations
1. **original function** The function we are hooking,
2. **hook function** The hook function that will run custom code.
3. **trampoline function** The dynamically created function that will restore code flow.
4. **stolen bytes/stolen instructions** The original function's bytes before our patch.

## Step-by-step process
1. Create the trampoline function by allocating memory. In 64-bit we allocate 12 bytes + the amount of stolen bytes. This will make sure we have enough space for the jump instruction and the stolen instructions to run.<br>
*Note: a 64-bit absolute jump is 12 bytes. In 32-bit you would typically do 5 bytes.*<br>
*Note 2: make sure to always take entire instructions and not leave any partial bytes behind.*
2. Populate the allocated trampoline function with the stolen bytes and finally the jump instruction. The jump instruction jumps back to the original function + stolen bytes.
3. Patch the original function's first 12 bytes with a jump to the hook function, and NOP (`0x90`) remaining bytes (if any) to remove partial instructions.
4. Store the trampoline function's address. In the hook function, we cast that address to a callable function-pointer and call it.
5. Once our hook function completes execution, it calls the trampoline function, which executes the stolen instructions and jumps back to the original code flow.

## Final product visualization
![Trampoline Hook Diagram](/diagram.svg)

`Tested on Counter-Strike Source Direct3D9 [FOR DEMONSTRATION PURPOSES ONLY]`

**🎉 Happy 2026!**