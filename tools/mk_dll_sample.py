# -*- coding: utf-8 -*-
"""写一个「文本形式」的易语言源码，用来在原版 e.exe 里打开、量 Dll命令 页的几何。

语法依据：refs\\OpenEpl\\TextECode 的 EplParser.g4：
    dllDeclareElem : .DLL命令 名, 返回值类型, "库文件名", "在库中对应命令名", 公开, 备注
                     (换行 .参数 名, 类型, 传址|参考, 数组, 备注)*
用法：python mk_dll_sample.py [输出路径]
"""
import sys

TXT = """.版本 2

.程序集 窗口程序集_启动窗口
.程序集变量 记录集, 整数型

.子程序 _启动窗口_创建完毕

.子程序 加法, 整数型, , 公开
.参数 a, 整数型
.参数 b, 整数型
    返回 (a ＋ b)

.DLL命令 ZwWow64ReadVirtualMemory, 整数型, "ntdll.dll", "ZwWow64ReadVirtualMemory", , 对象句柄
.参数 ProcessHandle, 整数型, , , 对象句柄
.参数 BaseAddress, 长整数型, , , 内存地址
.参数 Buffer, 字节集, , 数组, 返回缓冲区
.参数 BufferSize, 长整型, , , 写入大小
.参数 NumberOfBytesRead, 长整型, 传址, , 实际写入大小

.DLL命令 ZwWow64WriteVirtualMemory, 整数型, "ntdll.dll", "ZwWow64WriteVirtualMemory"
.参数 ProcessHandle, 整数型, , , 对象句柄
.参数 BaseAddress, 长整数型, , , 内存地址
.参数 Buffer, 字节集, , 数组, 返回缓冲区
.参数 BufferSize, 长整型, , , 写入大小
.参数 NumberOfBytesRead, 长整型, 传址, , 实际写入大小

.DLL命令 CopyMemory_DRAWPITEMSTRUCT, , "kernel32.dll", "RtlMoveMemory", 公开
.参数 Destination, DRAWPITEMSTRUCT, 传址, , 目标
.参数 Source, DRAWPITEMSTRUCT, 传址, , 源
.参数 Length, 整数型, , , 长度

.全局变量 全局计数, 整数型

.数据类型 类_识图, 公开
.成员 句柄, 整数型
.成员 名称, 文本型
"""


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else r"D:\eclone\shots\e_samples\dll_sample.e"
    import os
    os.makedirs(os.path.dirname(out), exist_ok=True)
    data = TXT.replace("\n", "\r\n").encode("gbk")
    open(out, "wb").write(data)
    print("wrote %s (%d bytes, GBK/CRLF)" % (out, len(data)))


if __name__ == "__main__":
    main()
