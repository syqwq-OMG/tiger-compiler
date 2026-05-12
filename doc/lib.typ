#import "@preview/itemize:0.2.0" as _itemize
#import "@preview/zebraw:0.6.1": zebraw
#import "@preview/physica:0.9.8": *
#import "@preview/gentle-clues:1.3.1": *

#let code-highlight-color = rgb("#2a61e2").transparentize(95%)
#let zcode = zebraw.with(background-color: luma(251), hanging-indent: true, indentation: 4, highlight-color: code-highlight-color)

#let report(
  name: "syqwq",
  course: "IAI",
  date: datetime.today(),
  tutor: "Tutor",
  id: 10234900421,
  exp-name: "exp name",
  grade: 2024,
  body,
) = {
  set text(font: ("New Computer Modern", "Source Han Serif SC"))

  set page(
    header: box(
      width: 100%,
      stroke: (bottom: luma(200) + .7pt),
      outset: 3pt,
      align(center, text(fill: luma(100))[华东师范大学数据科学与工程学院实验报告]),
    ),
    numbering: "1/1",
  )

  set heading(numbering: "I.1.1")
  set par(justify: true)
  show "。": "."
  set enum(numbering: "(1)")
  show: _itemize.default-enum-list.with(indent: .5em)
  show: zcode
  // show raw: set text(font: "Consolas Nerd Font")
  show raw: set text(font: "FiraCode Nerd Font")

  align(center, text(size: 17pt, weight: 600)[华东师范大学数据科学与工程学院实验报告])

  let ti(a, b) = [#strong(a): #b]
  table(
    columns: (1fr,) * 3,
    stroke: luma(220) + .5pt,
    inset: 7pt,
    ti("课程名称", course), ti("年级", grade), ti("上机实践时间", date.display("[year].[month].[day]")),
    ti("指导教师", tutor), ti("姓名", name), [],
    ti("上机实践名称", exp-name), ti("学号", id), [],
  )

  line(length: 100%)


  body
  /*
  = 实验任务

  = 使用环境

  = 实验过程

  = 总结
  */
}


/*
#table(
    columns: 2,
    align: (horizon,) * 2,
    rows: (auto, 2em),
    stroke: none,
    table.hline(y: 0, stroke: 2pt),
    table.hline(y: 1),
    table.hline(y: 7, stroke: 2pt),

    [*符号*], [*定义*],
    [$S_t$], [第 $t$ 回合的静态兵力评分],
    [$X$], [玩家投降时刻的兵力差],
    [$M_(i j)$], [兵种 $i$ 击杀兵种 $j$ 的频次],
    [$S e q$], [开局前 $T$ 步的着法序列],
    [$E_(1)$], [白方的 ELO 等级分],
    [$E_(2)$], [黑方的 ELO 等级分],
)
*/
