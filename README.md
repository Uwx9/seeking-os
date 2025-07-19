# Qiusuo_os
#### ——————操作系统真象还原 - 代码复现

## 项目背景

仅个人学习，希望能提升自己，与大家共勉。  
本项目是基于郑钢所著《操作系统真象还原》一书中的内容开发的操作系统实验代码复现。本书深入浅出地讲解了操作系统的核心机制，从零开始自制操作系统全流程，并提供了详尽的代码实现与运行环境。

## 环境

- bochs版本：2.6.8  
- 编译器：gcc version 4.8.5 (Ubuntu 4.8.5-4ubuntu2)  
- 汇编器：NASM version 2.14.02  
- 链接器：GNU ld (GNU Binutils for Ubuntu) 2.34  
- make：GNU Make 4.2.1  

## 仓库链接

更详细 commit 的仓库在 Gitee：  
<a href="https://gitee.com/yustarxin/Qiusuo_os" target="_blank">https://gitee.com/yustarxin/Qiusuo_os</a>  
很抱歉 commit 不规范且混乱。

将 bochs 和源码放在一起的仓库：  
<a href="https://gitee.com/yustarxin/qiusuo_os--packed" target="_blank">https://gitee.com/yustarxin/qiusuo_os--packed</a>

## 一些感悟

首先要感谢这本书的作者郑钢先生，这是一本极好的好书，可谓是手把手教学。  
用了近大半个学期的时间从 0 开始写完了一个操作系统，真的是收获良多。开始时困难重重，后面慢慢还是适应了。尤其是 debug 时不要太灰心，虽然 debug 可能有点折磨。  
同时我意识到自己此前的学习并不足够认真深入，此后要学习的还有很多，要多读很多书。很开心的是这个项目激发了我的热情让我渴望做出漂亮且有用的东西 ^^。  

## TODO
我希望能让这个mini操作系统更完善一点，后续想做的是实现网络，在物理机启动等功能，这也是一个很好的学习的机会。

### 关于第15章的bug
exec.c文件的load函数中刚写完时是有bug的，没有自己debug看的网上前辈们的解决方法但没有找到具体错误原因，修改后会因为回收不了旧的arena导致内存泄漏。
过了快两个月，本想再来debug一下这个诡异的bug，于是注释了修改的那一句代码，重新编译后居然能够正常加载进程，有点哭笑不得。哈哈，就当它解决了吧。
