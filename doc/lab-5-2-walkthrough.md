# Lab 5-2 Walkthrough: Code Generation

Great news! The Code Generation pass and Translation fixes for Lab 5-2 have been successfully completed, resulting in a **perfect 100/100 score** on the autograder. 

Here is a summary of the issues encountered and the fixes applied:

## 1. Short-Circuit Logic for Bitwise Operators
In `qsort.tig`, an infinite loop occurred because the Tiger language semantics dictate that `&` and `|` must short-circuit, just like `&&` and `||` in C.
- Previously, `AND_OP` and `OR_OP` were being treated as pure arithmetic expressions returning values `0` or `1`.
- **Fix**: We modified `tree::OpExp::Translate` in `translate.cc` to correctly generate control flow (`Cx`) instead of value expressions (`Ex`) for `AND_OP` and `OR_OP`. This implemented the required short-circuiting logic and allowed `qsort.tig` to branch properly.

## 2. Static Link Location Bug
In tests like `merge.tig`, we saw failing behaviors related to the formal parameters stack mismatch.
- When creating a new level for a function in `Level::NewLevel` (`translate.h`), we pushed the "static link escape flag" (`true`) to the **end** of the formal parameters list instead of the **beginning**.
- **Fix**: The static link is always the first parameter in the Tiger calling convention, so we changed `formals->push_back(true)` to `formals->push_front(true)`.

## 3. Frame Alignment and Stack Access Bug
The final failing test was `test_patch.tig`, which threw a mysterious `KeyError: 't118'`. The compiler was incorrectly computing stack locations for arguments passed on the stack.
- Due to a misunderstanding of where the Frame Pointer (FP) is simulated (`%rsp + framesize` at function entry), stack arguments were being accessed at offsets `16(FP)`, `24(FP)`, `32(FP)`. However, since FP points to the *return address*, the first stack argument (7th argument overall) is actually at `8(FP)`, the second at `16(FP)`, etc.
- **Fix**: Corrected the starting offset in `NewFrame` (`x64frame.cc`) from `formal_offset + 16` to `formal_offset + 8`.

## 4. Unallocated Outgo Space Overwriting Return Addresses
Also in `test_patch.tig`, we diagnosed an obscure crash where `tigermain` returned from the program, but resumed executing code randomly within `patchtest`.
- `tigermain` was allocating 0 bytes for outgoing arguments, directly writing to `0(%rsp)`, `8(%rsp)`, etc., which overwrote its own return address!
- **Fix**: Implemented tracking of `outgo_size_` by declaring `static frame::Frame *current_frame` in `codegen.cc` (since Tiger compiles in a single pass), which gets set to the active frame in `CodeGen::Codegen()`. Then, inside `ExpList::MunchArgs()`, we invoke `cg::current_frame->AllocOutgoSpace(...)` based on the number of arguments (minus 6) passed in function calls. This shifts the `%rsp` appropriately during the `callq`, preserving the function's own return address on the stack.

## Result
With these 4 tricky edge-cases resolved, the codebase properly generates accurate and robust X86-64 assembly.
All 20 tests pass completely with `make gradelab5-2` yielding `LAB5 part2 SCORE: 100`.
