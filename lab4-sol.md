# Lab 4 Type Checking (语义分析与类型检查) 实验总结与答辩指南

## 1. 实验目标
本实验的核心是为 Tiger 编译器实现**语义分析（Semantic Analysis）**阶段，其主要任务是对抽象语法树（AST）进行遍历，执行**类型检查（Type Checking）**并**维护符号表（Environments）**，在发现类型不匹配、未定义变量等语义错误时及时报错。

相关的主要代码实现集中在 `src/tiger/semant/semant.cc`。

---

## 2. 核心知识点与数据结构

为了应对答辩，你需要充分理解以下几个核心概念：

### 2.1 符号表与环境（Environments）
在 Tiger 语言中，存在两个分离的命名空间（环境）：
- **类型环境 (`TEnv`)**：映射 `Symbol` 到具体的类型 `type::Ty`。例如 `type myInt = int` 会在 `TEnv` 中插入 `myInt -> IntTy`。
- **值环境 (`VEnv`)**：映射 `Symbol` 到具体的变量或函数信息 `env::EnvEntry`。
  - `VarEntry`：存储变量的类型 `ty_`，以及是否只读的标记 `readonly_`。
  - `FunEntry`：存储函数的参数类型列表 `formals_` 和返回值类型 `result_`。

**为什么需要两个环境？**
因为 Tiger 语言允许变量/函数名和类型名重名。例如 `var a: a := ...`，前者在 `VEnv` 查找，后者在 `TEnv` 查找，互不干扰。

### 2.2 类型的实际解析 (`ActualTy()`)
Tiger 语言允许**类型别名**，比如：
```tiger
type a = int
type b = a
```
在代码中，这会产生嵌套的 `NameTy`。当我们需要判断两个类型是否匹配，或者确认某个类型是不是真正的数组/记录时，我们不能直接判断它是不是 `NameTy`，而是要使用 `ActualTy()` 方法“剥开”外层的别名，直到找到底层的真实结构（如 `IntTy`、`RecordTy` 等）。

---

## 3. 核心节点类型检查逻辑 (AST 遍历)

语义分析的过程就是调用各种 AST 节点的 `SemAnalyze` 方法的过程。下面是几个最典型的实现思路：

### 3.1 变量查询 (Var)
- **`SimpleVar`**：去 `VEnv` 中查找变量名字。如果找不到，报错 `"undefined variable %s"`；如果找到了，返回它的实际类型。
- **`FieldVar` (记录体字段访问 `a.b`)**：先对变量 `a` 进行 `SemAnalyze` 得到其类型，通过 `ActualTy()` 确保它是一个 `RecordTy`（否则报错 `"not a record type"`），然后在该记录的 `fields` 列表中遍历查找是否存在名为 `b` 的字段。
- **`SubscriptVar` (数组下标访问 `a[i]`)**：对 `a` 检查是否为 `ArrayTy`，对 `i` 检查是否为 `IntTy`。

### 3.2 表达式检查 (Exp)
- **运算表达式 (`OpExp`)**：
  - 算术运算 (`+`, `-`, `*`, `/`)：左右操作数都必须严格是 `IntTy`。
  - 比较运算 (`=`, `<>`)：左右操作数类型必须通过 `IsSameType()` 检查。特别注意，`nil` 只能与 `RecordTy` 比较，表示空指针。
- **函数调用 (`CallExp`)**：
  在 `VEnv` 查找函数签名。然后**同步遍历**实参列表和形参列表。如果个数不匹配则报错 `"too few/many params in function"`。如果参数类型不匹配，则报错 `"para type mismatch"`。
- **控制流表达式 (`IfExp`, `WhileExp`, `ForExp`)**：
  - 所有的条件判断（test）必须是 `IntTy`。
  - `While` 和 `For` 的循环体 (`body`) 规定不能有返回值，因此其类型必须为 `VoidTy`。
  - **只读变量保护**：在 `ForExp` 中，循环迭代变量在循环体内是不允许被修改的。我们在进入 `For` 的 `body` 作用域前，往 `VEnv` 压入该迭代变量，并标记其为只读（`readonly_ = true`）。在赋值语句 `AssignExp` 中如果碰到给它赋值，就抛出 `"loop variable can't be assigned"`。

### 3.3 声明检查 (Dec) - [重点与难点]
在 Tiger 中，相邻的同类声明可以互相引用（即**互递归**）。

- **变量声明 (`VarDec`)**：
  如果带有显式类型标注（如 `var a: int := 1`），要检查右侧初始化表达式类型与标注是否一致。
  **特殊情况**：如果不带类型标注直接初始化为 `nil`（`var a := nil`）是非法的，因为编译器无法推断 `a` 究竟是哪种类型的指针。此时报错 `"init should not be nil without type specified"`。

- **函数互递归 (`FunctionDec`)**：
  分为**两趟（Two Passes）**处理：
  1. **第一趟**：把所有函数的“签名（参数类型、返回值类型）”提取出来，放入 `VEnv`。同时检查这一批声明中是否有重复的函数名（`"two functions have the same name"`）。
  2. **第二趟**：深入遍历每个函数的 `body`。因为第一趟已经把所有的函数签名放入了环境，所以在 `body` 中遇到对同批次函数的互相调用时，查表就不会失败。还要检查函数的返回值类型是否和签名一致。

- **类型互递归 (`TypeDec`)**：
  这部分最为复杂，分为**三趟（Three Passes）**：
  1. **填表**：将所有的类型名称以悬空的 `NameTy(name, nullptr)` 形式放入 `TEnv`。检查重名。
  2. **解析**：对每一个类型的真实定义进行解析，并把解析结果填入刚才悬空的 `NameTy` 的 `ty_` 指针里。
  3. **成环检测 (Cycle Detection)**：由于 Tiger 允许互递归，恶意代码可能会写出死循环类型：`type a = b; type b = a`。我们在第三趟中顺着 `NameTy` 的链条往下找，如果发现回到了起点（指针相同），则抛出 `"illegal type cycle"` 错误。

---

## 4. 答辩常见问题准备 (Q&A)

**Q1：为什么 Record 和 Array 在做等价判断（`IsSameType`）的时候只是比较指针地址？**
**答**：在 Tiger 语言规范中，Record 和 Array 是按**名称等价 (Name Equivalence)** 而不是结构等价来判断的。每一次记录或数组的显式声明都会在内存中生成一个新的、独一无二的类型实例。因此只要两个类型的指针不指向内存中的同一个 `RecordTy` 或 `ArrayTy` 实例，它们就是不同的类型。

**Q2：如果出现 `type a = int; type b = a;`，当你判断 `b` 是不是 `int` 时内部是怎么做的？**
**答**：对 `b` 调用 `ActualTy()`。`ActualTy` 函数内部是一个递归或者 while 循环，当遇到类型是 `NameTy` 时，它会继续沿着指针去寻找底层的真实类型，跳过 `b` 找到 `a`，再跳过 `a` 找到内置的 `IntTy` 并返回，从而正确识别其底层数据类型。

**Q3：`LetExp` 表达式返回的类型是什么？如果 `LetExp` 的 `in...end` 之间没有语句呢？**
**答**：`LetExp` 的类型就是它最后 `body`（通常是 `SeqExp` 顺序语句）执行完毕后的类型。如果 `body` 为空，说明没有产生任何有意义的值，按照规范应返回 `VoidTy`。我们在分析前会执行 `BeginScope()`，分析结束后 `EndScope()`，从而严格保证其中声明的局部变量不会污染外层。

**Q4：For 循环的迭代变量是如何保证不被修改的？**
**答**：在进入 `ForExp` 的 `body` 前，将迭代器变量作为 `VarEntry` 注册进当前作用域的 `VEnv` 中，并将 `VarEntry` 的布尔值 `readonly_` 设置为 `true`。当语义分析来到 `AssignExp` 时，不仅会检查左右类型是否匹配，还会额外判断左值如果是一个 `SimpleVar`，它在 `VEnv` 中对应的 `readonly_` 是否为 `true`。若是，则抛出错误 `"loop variable can't be assigned"`。
