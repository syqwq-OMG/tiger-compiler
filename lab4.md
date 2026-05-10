Lab4 Type Checking - 26sp 

**Deadline:** 2026/5/13 

---

1. Lab Description 
   Write a type-checking phase for your compiler, the module semant describes this phase. Related files for this lab are: 

- **src/tiger/semant/semant**: The `SemAnalyze` function implementations of all AST classes. 
- **src/tiger/semant/type**: A series of `Type` classes describe types in tiger language. 
- **src/tiger/env/env**: The `EnvEntry` classes which help the compiler store the information of variables and functions when doing type checking. 
  To finish this lab, the only file you'll need to fill is `semant.cc`, you can change the declarations of function `SemAnalyze` in `semant.h` as you wish. 
  Hints: 
  Try to use member `errormsg_` in `ProgSem` to report semantic errors you detect. More specifically, since `errormsg_` is a private member of `ProgSem`, it cannot be accessed directly. Instead, use the `Error` interface of the `errormsg` parameter to report semantic errors. 

---

2. Environment 
   You will use the same code framework you set up when you worked on Lab 3 and the code you wrote in Lab 3. What you need to do now is to pull the latest update of the code framework. You may have to do some code-merging jobs. If you have any difficulties in merging codes, you can ask TAs in our Wechat group. 

```bash
// May you be familiar with the basic git operations. [cite: 18]
// If you have any change that hasn't been committed, [cite: 20]
// first git add and git commit, then do the following operations. [cite: 22]

> shell% [cite: 24]
git fetch upstream [cite: 26]

> shell% [cite: 28]
git checkout -b lab4 upstream/lab4 [cite: 30]

> shell% [cite: 31]
git merge lab3 [cite: 31]
// You may have to do some code-merging jobs here. [cite: 31]
// After merging you are supposed to begin your work on lab4 [cite: 31]

```

---

3. Grade Test 
   The lab environment contains a grading script named as `grade.sh`, you can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands. 

```bash
> shell% [cite: 37]
make gradelab4 [cite: 39]

> [^_^]: Pass Lab4 #If you pass all the tests, you will see these messages. [cite: 44]
> TOTAL SCORE: 100 [cite: 44]

```

---

4. Submission Instructions 

5. Use `make ziplab4` command to generate the zip file. 
6. Hand in the zip file to gradescope website. 

### Gradescope Details

- **Assignment Name:** Lab4 Type Checking 
- **Total Points:** 100.0 
- **Availability:** APR 15, 2025 11:59 AM to APR 30, 2025 11:59 PM 
- **Late Due Date:** MAY 21, 2025 11:59 PM 
  ! Remember to submit before the deadline to avoid late penalties!