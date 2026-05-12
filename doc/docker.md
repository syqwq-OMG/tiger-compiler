docker run -it --privileged -p 2121:22 lyf726/tiger-compiler-env:latest 这个是运行指令

docker ps -a

docker start [id]

docker exec -it [id] bash


Ran command: `docker ps -a`
Ran command: `docker exec -it 8ce9c641c971 bash
`
Ran command: `docker exec -it 995f809165b7 bash
`
Ran command: `docker start 995f809165b7`
Viewed docker.md:1-7
Edited docker.md

因为你最初运行 `docker run -it --privileged -p 2121:22 lyf726/tiger-compiler-env:latest` 这条命令时，**没有加上挂载目录的参数**。

Docker 容器的特性是**完全隔离**的。你可以把它想象成一台全新安装的虚拟电脑，如果你不特意告诉它“把外面宿主机的某个文件夹映射到里面”，它里面自然是空空如也，完全看不到你本地的 `tiger-compiler` 代码文件夹。

### 怎么解决这个问题？

由于你现在通过 `docker start` 和 `docker exec` 进入的旧容器 `995f809165b7` 里是没有代码的，我建议你直接新开一个正确挂载了目录的容器。

请你在本地电脑的终端（不是在 Docker 里面）按顺序运行下面两步：

**第一步：启动一个挂载了你代码目录的新容器**
在 `/home/syqwq/Workspace/tiger-compiler` 目录下运行这行长长的指令：

```bash
docker run -it --privileged -v /home/syqwq/Workspace/tiger-compiler:/home/stu/tiger-compiler -w /home/stu/tiger-compiler lyf726/tiger-compiler-env:latest bash
```

**第二步：进去之后直接编译**
当上面的命令执行后，你的命令行前面会变成类似 `stu@xxxxxx:~/tiger-compiler$`，说明你已经进去了。
此时你再敲 `ls`，就会惊喜地发现你的代码都在里面了！
然后你就可以直接跑测试脚本了：
```bash
make gradelab4
```

> **原理说明：** 
> 关键在于加上的这句 `-v /home/syqwq/Workspace/tiger-compiler:/home/stu/tiger-compiler`。
> 它像一座桥，把**外面的文件夹**和**里面的文件夹**连通了。你在外面用编辑器修改了代码，里面也会立刻生效。后续你就保持用这种带 `-v` 的方式启动容器即可。