Lab6 Register Allocation 26sp   

**Deadline:** 2026/6/24 23:59   

1. Lab Description   

Finish register allocation in your tiger compiler.   

Related files for this lab are:   

- `src/tiger/liveness/.*` Files related to liveness analysis   
- `src/tiger/regalloc/.*` Files related to register allocation   

To finish this lab, you will only need to finish the following modules: { liveness analysis } { register allocation }, you can modify the following files to finish your design.   

```
.── src/tiger/parse/tiger.y
├── src/tiger/lex/tiger.lex
├── src/tiger/lex/scanner.h
├── src/tiger/absyn/absyn.*
├── src/tiger/semant/semant.*
├── src/tiger/escape/escape.*
├── src/tiger/frame/frame.h
├── src/tiger/frame/temp.*
├── src/tiger/frame/x64frame.*
├── src/tiger/translate/translate.*
├── src/tiger/translate/tree.*
├── src/tiger/canon/canon.*
├── src/tiger/codegen/assem.*
├── src/tiger/codegen/codegen.*
├── src/tiger/liveness/flowgraph.*
├── src/tiger/liveness/liveness.*
├── src/tiger/regalloc/color.*
├── src/tiger/regalloc/regalloc.*
├── src/tiger/util/graph.h
├── src/tiger/util/table.h
```
In your generated code, you should do what you can to avoid unnecessary stack access (push and pop). This means using escape analysis to identify variables that can be kept in registers before the register allocation phase.   

> **Notice:** Before you start this lab, you should carefully read chapters 10, 11 of the textbook.   

2. Implementation Hints   

Now that you've completed register allocation for your Tiger compiler, the generated assembly can be linked with gcc to produce a real executable that runs on actual hardware.   

The testing process for this lab includes the following steps:   

1. Build the tiger-compiler executable.   
2. Use tiger-compiler to compile a Tiger source file into assembly.   
3. Link the generated assembly with the provided runtime library using gcc   
4. Run the resulting executable and compare its output with the expected reference output.   

During register allocation, follow these main steps:

a. Construct the control flow graph (CFG). 
b. Perform liveness analysis on the CFG to determine where each temporary is live. 
c. Build the interference graph, where an edge indicates two temporaries cannot share a register, according to the result of liveness analysis. 
d. Apply graph coloring to assign physical registers to temporaries. 
e. If there are not enough registers, mark some for spilling, rewrite the program to include load/store instructions, and repeat the analysis. 

3. Environment   

Merge the code from lab5-2, and update the branch lab6 from gitee.   

4. Local Grade Test   

The lab environment contains a grading script named `grade.sh`. You can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands.   

```
~/tiger-lab6 > bash ./scripts/grade.sh lab6
Pass bsearch
Pass dec2bin
Pass merge/test1
Pass merge/test2
Pass merge/test3
Pass merge/test4
Pass prime
Pass qsort
Pass queens
Pass tbi
Pass test_array
Pass test_patch
Pass tfact
Pass tfo
Pass tif
Pass tifn
Pass tlink
Pass trec
Pass tree
Pass twhi
[^_^]: Pass
LAB6 SCORE: 100
```

5. Submission Instructions   

1. Use `make ziplab6` command to generate the zip file.   
2. Do not change any files other than those included in the `make ziplab6` command.   
3. Hand in the zip file to Gradescope website.   



**Lab6 Register Allocation Submission Windows:**   

- **Open:** MAY 27, 2025 11:59 AM   
- **Due:** JUN 16, 2025 11:59 PM   
- **Late Due Date:** JUN 22, 2025 11:59 PM   