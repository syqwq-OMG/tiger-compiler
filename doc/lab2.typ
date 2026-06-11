#import "lib.typ": *

#show: report.with(
  course: "编译原理",
  tutor: "黄波",
  exp-name: "Lab 2",
  name: "孙育泉",
)

= 实验任务
使用 flexc++ 实现一个针对 tiger 语言的 lexical scanner，要求包含以下部分：

- Basic specifications

- Comments handling

- String handling

- Error handling

= 实验过程

== 具体实现

首先，除了 `ID`, `INT`, `STRING` 等特殊的 token 以外，我们只需要调用 `adjust()` 来进行正常读取，然后返回对应的 `Parser` 即可。

```lex
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"." {adjust(); return Parser::DOT;}
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
```

接下来，对于数字，由于定义了 `digit [0-9]`，因此我们可以直接使用 `digit+` 来匹配一个或多个数字，并将其转换为整数返回：

```lex
{digit}+ {
    adjust();
    return Parser::INT;
}
```

接下来，对于标识符，由于 Tiger 语言中可以允许数字、字母、下划线，但是要求字母开头，于是可以写出正则表达式：

```lex
{letter}({letter}|{digit}|_)* {
    adjust();
    return Parser::ID;
}
```
接下来处理注释部分。
在 Tiger 语言中，注释是可以嵌套的，因此我们需要使用独占状态 ` COMMENT` 以及一个变量 `comment_level_` 来专门记录嵌套的层级。

当词法分析器遇到 `/*` 时，会初始化嵌套层级为 1，并进入 `COMMENT` 状态：

```lex
"/*" {
    adjust();
    // 记录注释起始位置
    comment_level_ = 1;    // 初始化嵌套层级
    begin(StartCondition_::COMMENT);
}
```

在 `<COMMENT>` 状态内部，我们需要处理以下几种情况：

- *嵌套加深*：如果再次遇到 `/*`，则将 `comment_level_` 递增，嵌套层级加深。

- *嵌套变浅与退出*：如果遇到 `*/`，则 `comment_level_` 递减；当层级归零（`comment_level_ == 0`）时，说明最外层注释已闭合，此时退回 `INITIAL` 正常状态。

- *换行符处理*：遇到 `\n` 时，必须调用 `errormsg_->Newline()` 通知错误处理模块，以保证后续报错时的行号准确无误。

- *普通字符处理*：对于注释内的其他所有普通字符 `.`，仅调用 `adjustStr()` 忽略即可。

- *异常处理*：如果遇到了文件结束符 `<<EOF>>` 但注释依然没有闭合，则调用 `errormsg_->Error` 报错 "unclosed comment"，并强制退回初始状态。

```lex
<COMMENT>{
    "/*" {
        adjustStr();       // 内部字符，只移动指针
        comment_level_++;  // 嵌套层级加深
    }
    "*/" {
        adjustStr();
        comment_level_--;  // 嵌套层级变浅
        if (comment_level_ == 0) {
            begin(StartCondition_::INITIAL); // 退回正常状态
        }
    }
    \n {
        adjustStr();
        errormsg_->Newline(); // 换行必须通知 errormsg_
    }
    . {
        adjustStr();       // 忽略注释内的其他所有普通字符
    }
    <<EOF>> {
        // 如果文件结束了但注释还没闭合，要报错
        adjustStr();
        errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");
        begin(StartCondition_::INITIAL);
    }
}
```

然后，处理字符串部分。

字符串的处理同样依赖于独占状态 `STR` 。当遇到双引号 `"` 时，首先调用 `string_buf_.clear()` 清空之前遗留的缓冲区，并进入 `STR` 状态。



在 `<STR>` 状态中，最为核心的是各种转义序列的解析：

+ *基础转义字符*：例如 `\\n`, `\\t`, `\\\\`, `\\\"`，匹配到后直接向 `string_buf_` 中追加对应的真实字符 `\n`, `\t`, `\\`, `\"`。

+ *三位十进制 ASCII 码 (`\ddd`)*：通过截取匹配字符串的后三位数字，使用 `std::stoi` 将其转换为整数，再通过 `static_cast<char>` 强制转换后追加到缓冲区。


+ *控制字符 (`\^c`)*：提取出匹配字符串中的字母，利用位运算技巧（与 31 进行按位与运算，即 `&31`）直接将其转换为对应的控制字符 ASCII 码。

+ *多行字符串格式化*：Tiger 语言允许使用 `\\[ \t\n\f]+\\` 来忽略多行字符串间的空白符。此时需要遍历匹配到的字符串，并在遇到换行符时调用 `errormsg_->Newline()` 以维持准确的行号。

对于字符串中的常规字符，直接追加匹配到的首字符即可。 如果遇到未转义的闭合双引号 `"`，则调用 `setMatched(string_buf_)` 将匹配的字面量替换为拼接好的真实字符串，退出 `STR` 状态并返回 `Parser::STRING`.

如果在字符串中遇到了非法的原生换行符 `\n`，报错 `"illegal newline in string"` ；如果遇到 `<<EOF>>` 而字符串未闭合，则报错 `"unclosed string"`.

```lex
/* 遇到双引号，进入字符串状态 */
\" {
    adjust();              // 记录字符串起始位置 (tok_pos_ 留在这里)
    string_buf_.clear();   // 清空之前遗留的 buffer
    begin(StartCondition_::STR);
}

 /* 在 STR 状态下处理内部逻辑 */
<STR>{
    \" {
        adjustStr();
        setMatched(string_buf_); // 核心：把 flex 匹配到的字面量替换为你拼接的真实字符串
        begin(StartCondition_::INITIAL);
        return Parser::STRING;
    }

    /* 处理转义字符 */
    \\n { adjustStr(); string_buf_ += '\n'; }
    \\t { adjustStr(); string_buf_ += '\t'; }
    \\\\ { adjustStr(); string_buf_ += '\\'; }
    \\\" { adjustStr(); string_buf_ += '\"'; }

    /* 处理 \ddd (三位十进制 ASCII 码) */
    \\[0-9]{3} {
        adjustStr();
        std::string text = matched();
        // text 是类似 "\099" 的字符串。我们要截取从索引 1 开始的 "099"，转成整数，再强制转换成 char
        int ascii_val = std::stoi(text.substr(1)); 
        string_buf_ += static_cast<char>(ascii_val);
    }

    /* 处理 \^c (控制字符) */
    \\\^[a-zA-Z] {
        adjustStr();
        char control_char = matched()[2]; // 取出匹配字符串 "\^C" 里的第 3 个字符，即 'C'
        // 一个非常优雅的位运算技巧：大写字母减去 64 刚好就是控制字符的 ASCII 码，或者直接与 31 按位与
        string_buf_ += static_cast<char>(control_char & 31);
    }
    
    
    \\[ \t\n\f]+\\ {
        adjustStr();
        std::string matched_str = matched();
        for (char c : matched_str) {
            if (c == '\n') {
                errormsg_->Newline();
            }
        }
    }
    
    \n {
        adjustStr();
        errormsg_->Error(errormsg_->tok_pos_, "illegal newline in string");
        errormsg_->Newline();
    }
    
    /* 处理普通字符 */
    . {
        adjustStr();
        string_buf_ += matched()[0]; 
    }
    
    <<EOF>> {
        adjustStr();
        errormsg_->Error(errormsg_->tok_pos_, "unclosed string");
        begin(StartCondition_::INITIAL);
    }
}
```


最后，在正常的 `INITIAL` 状态下，我们需要将空格和制表符 `[ \t]+` 过滤掉并调整指针位置 。遇到换行符 `\n` 时，除了调整位置外，必须调用 `errormsg_->Newline()` 更新行号记录。


对于任何不匹配上述所有规则的非法输入，则直接通过 `errormsg_->Error(errormsg_->tok_pos_, "illegal token")` 抛出非法词法单元的错误.

== 遇到的问题

+ 第一次写的时候，忽略了变量的名字可以出现下划线，导致有些测试样例没有通过。

+  `lex` 文件写规则是有优先级的，第一次写的时候，把数字和标识符的规则写在了最前面，导致关键字都被错误地识别为普通的变量名。

+ 实现字符串的时候，没有实现控制字符。