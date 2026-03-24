%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
digit [0-9]
letter [a-zA-Z]

%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */


/*reserved word and symbols */
"array" {adjust(); return Parser::ARRAY;}


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


{digit}+ {
    adjust();
    return Parser::INT;
}

{letter}({letter}|{digit}|_)* {
    adjust();
    return Parser::ID;
}




/* 遇到 /* 进入注释状态 */
"/*" {
    adjust();              // 记录注释起始位置
    comment_level_ = 1;    // 初始化嵌套层级
    begin(StartCondition_::COMMENT); 
}

/* 在 COMMENT 状态下处理内部逻辑 */
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

/* TODO: Put your lab2 code here */

/*
* skip white space chars.
* space, tabs and LF
*/

[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
