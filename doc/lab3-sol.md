# 编译原理 Lab 3 (Parser) 实验指南与答辩准备

## 1. 实验核心目标与知识点解析

在前面的 Lab 2 中，我们使用 Flexc++ 实现了词法分析器 (Lexical Scanner)，将源代码字符串转换成了 `Token`。在本次 Lab 3 中，我们的任务是**使用 Bisonc++ 实现语法分析器 (Syntax Parser)**，它的作用是接受这些 Token，验证它们是否符合 Tiger 语言的语法规则，并在这个过程中构建出**抽象语法树 (AST)**。

### 1.1 什么是 AST？为什么要构建 AST？
抽象语法树 (Abstract Syntax Tree) 是源代码语法结构的树状表现形式。在代码中，`absyn.h` 定义了所有 AST 节点的类型（例如 `VarExp` 表示变量表达式，`LetExp` 表示 Let 表达式）。Parser 生成 AST 后，后续的语义分析 (Semantic Analysis) 和代码生成 (Code Generation) 都可以直接在这棵树上进行遍历操作，而不需要再去处理繁琐的 Token。

### 1.2 Bisonc++ 的工作原理 (LALR(1) 分析)
Bisonc++ 生成的是 LALR(1) (Look-Ahead Left-to-Right Rightmost Derivation) 解析器。这是一种自底向上 (Bottom-Up) 的分析方法：
- **Shift (移进)**：Parser 读取一个 token 放入状态栈。
- **Reduce (归约)**：当栈顶的几个 token 符合某条语法规则时，将它们“合并”为左侧的非终结符，并在此时执行我们在 `{}` 中编写的 C++ 动作（也就是构建 AST 节点的操作）。

---

## 2. 实验详细实现步骤

### 2.1 引入 Token 与定义优先级 (消除二义性)

在 `tiger.y` 中，首先需要声明我们在 `lex` 阶段返回的 Token，并分配它们的**结合性和优先级**。这对于解决 **Shift/Reduce 冲突** 至关重要。

```yacc
%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ARRAY IF WHILE FOR TO LET IN END BREAK NIL
  FUNCTION VAR TYPE

 /* 运算符优先级声明 (从低到高) */
%nonassoc THEN DO OF ASSIGN
%nonassoc ELSE
%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS
```

**答辩重点考点：悬挂 Else (Dangling-Else) 问题是如何解决的？**
> 当遇到 `if a then if b then c else d` 时，`else` 到底应该和哪个 `if` 结合？
> 在我们的规则中，我们给 `ELSE` 设定了比 `THEN` 更高的优先级。当 Parser 处理到内部的 `if b then c`，且下一个 Lookahead Token 是 `ELSE` 时：
> 1. 可以选择 Reduce (`if b then c` 归约为 `ifexp`)。
> 2. 可以选择 Shift (把 `ELSE` 移进栈)。
> 
> 由于 `ELSE` 优先级更高，Bisonc++ 会选择 **Shift**，从而将 `else` 与**最近的** `if` (也就是 `if b`) 结合，完美解决了二义性。

---

### 2.2 表达式 (Expressions) 的解析

表达式是 Tiger 语言的核心。我们需要使用 `absyn.h` 中的接口构造各种 AST 节点：

```yacc
exp:
    lvalue { $$ = new absyn::VarExp(scanner_.GetTokPos(), $1); }
  | INT { $$ = new absyn::IntExp(scanner_.GetTokPos(), $1); }
  | opexp { $$ = $1; }
  /* 其他表达式... */
```

对于算术表达式，动作代码非常直观：
```yacc
opexp:
    exp PLUS exp { $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::PLUS_OP, $1, $3); }
```

**答辩重点考点：一元负号 (Unary Minus) 的实现技巧**
一元负号 `-exp` 很容易和二元减法 `exp - exp` 混淆。我们通过 `%prec UMINUS` 伪优先级符号，给一元减号赋予极高的优先级：
```yacc
  | MINUS exp %prec UMINUS { 
      $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, 
                            new absyn::IntExp(scanner_.GetTokPos(), 0), $2); 
    }
```
我们将 `-exp` 巧妙地转化为 `0 - exp` 构建 AST 节点，既利用了现有的 `OpExp` 结构，又保证了正确性。

---

### 2.3 Lvalue (左值) 与数组创建的冲突处理

在 Tiger 语法中，变量可以具有以下形式：
1. 简单变量：`id`
2. 记录字段：`lvalue . id`
3. 数组下标：`lvalue [ exp ]`

然而，**数组创建** 的语法是 `id [ exp ] of exp`。
如果你仔细看这两种语法，在读取到 `id [ exp ]` 的时候，Parser 并不知道自己到底是在读取一个普通的数组左值，还是在创建一个新数组。只有在读到 `]` 后面的那个 token 是不是 `OF` 才能决定！但是 Bisonc++ 作为 LALR(1) Parser，只能往前看 (Look-Ahead) 1 个 Token。这就会导致无法解决的 Shift/Reduce 冲突。

**解决方案：延迟归约 (Delayed Reduction)**
我们将 `lvalue` 的构建策略略微改写，把单纯的 `ID` 与复杂的下标拆开：
```yacc
lvalue:
    ID  {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
  | lvalue_non_id  {$$ = $1;}
  ;

lvalue_non_id:
    lvalue DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);}
  | ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1), $3);}
  | lvalue_non_id LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);}
  ;
```
这样一来，当 Parser 看到 `ID LBRACK exp RBRACK` 后，如果要构建数组创建 `ArrayExp`，就不用提前去将 `ID` 归约为 `lvalue`，而是直接平滑过渡，这是一种经典的避开 LALR 局限性的文法重写技巧。

---

### 2.4 声明 (Declarations) 与 Mutually Recursive Blocks

在 Tiger 语言中，连续的类型声明 (Type Declarations) 或是函数声明 (Function Declarations) 可以相互递归调用。这就要求在 AST 中，必须将**相邻的同类声明打包放在同一个 Block 节点中** (比如 `FunctionDec` 或 `TypeDec` 中包含一个 `List`)。

```yacc
decs_nonempty:
    decs_nonempty_s { $$ = new absyn::DecList($1); }
  | decs_nonempty_s decs_nonempty { $$ = $2->Prepend($1); }
  ;

tydec:
    tydec_one { $$ = new absyn::NameAndTyList($1); }
  | tydec_one tydec { $$ = $2->Prepend($1); }
  ;
```

**答辩重点考点：为什么会有那 3 个无法消除的 Shift/Reduce 冲突？**
> 当我们解析连续的类型声明：`TYPE a = int  TYPE b = int` 时，遇到第二个 `TYPE`：
> - Parser 可以选择**归约**前一个 `tydec` 作为一个独立的块。
> - Parser 也可以选择**移进 (Shift)**，继续把第二个 `TYPE` 吃进去，和前一个组合在一起。
> 
> LALR(1) 在这里会报出 Shift/Reduce 冲突，但 Bison 默认的处理方式是 **偏好 Shift (Prefer Shift)**。这恰好满足了我们的要求：**尽可能多地把连续的声明放在同一个相互递归的块里**！因此这 3 个 S/R 冲突（1 个来自 TYPE，2 个来自 FUNCTION）是安全且不可避免的。

---

### 2.5 LET 表达式与其 Body 的巧妙处理

`LET decs IN expseq END` 结构中，`IN` 和 `END` 中间的代码不仅可以是一条表达式，也可以是用分号隔开的多条表达式。
我们需要特别注意的是：Appel 老师在定义 Tiger AST 时，对于 Let 表达式的 body 有强制的要求——它必须最终被包装在一个 `SeqExp` 里面，哪怕里面什么都没有！

```yacc
let_body:
    /* empty */ { $$ = new absyn::SeqExp(scanner_.GetTokPos(), new absyn::ExpList()); }
  | exp { $$ = new absyn::SeqExp(scanner_.GetTokPos(), new absyn::ExpList($1)); }
  | sequencing { $$ = new absyn::SeqExp(scanner_.GetTokPos(), $1); }
  ;
```
而对于普通的括号表达式 `(exp)`，它只能是 `exp` 本身，不能被包装，这就使得我们不能简单地复用 `let_body`，而是需要单独区分：
```yacc
expseq:
    LPAREN RPAREN { $$ = new absyn::VoidExp(scanner_.GetTokPos()); }
  | LPAREN exp RPAREN { $$ = $2; }
  | LPAREN sequencing RPAREN { $$ = new absyn::SeqExp(scanner_.GetTokPos(), $2); }
  ;
```

---

## 3. 答辩灵魂拷问准备

1. **你代码里满天飞的 `$1`, `$2`, `$$` 是什么？**
   - `$1`, `$2` 代表产生式右侧第 1、2 个符号所对应的语义值。
   - `$$` 代表产生式左侧（规约后）要向上传递的语义值。它们在底层的 C++ 类型正是我们在 `%union` 中定义的那些指针。

2. **你如何解决列表的生成？(例如参数列表、声明列表)**
   - 采用右递归/左递归均可。由于题目要求生成标准的 AST，我们使用了 `List->Prepend(Item)` 接口。
   - 例如 `exp SEMICOLON sequencing` 中，因为我们是从前往后解析，但在建立树节点时，借助 `Prepend` 能够非常直观地保证列表元素的顺序一致。

3. **对于 `nil` 和 `()`，你的 AST 是怎么表示的？**
   - `nil` 被映射到了 `NilExp`。
   - `()` 本质上代表空，映射为了专门补充的 `VoidExp`。

---
祝答辩顺利，享受这个手写 AST 构造的过程吧！
