// ec_eexpr.h -- `.e` 子程序正文（expression 字节码）解码 + 渲染成 eclone 的源码行。
//
// 规格来源：`D:\EPL\_ida\SEGMENT_SPEC.md` §3.6（已用 321 个子程序验证，consumed==len 321/321）。
// 权威依据：eplc-master/include/EplAst.hpp 的 EnumEplTokenType + utils.hpp 的 EplSystemId。
// ⚠️ 别用 eplc2\epl-translator\src\statement.cpp，它不完整且有错。
//
// 关键结论（都实测）：
//   - expression = 「1 字节 token + 该 token 的操作数」的语句流
//   - 0x6A = 命令语句头，后跟 16B 头 [u32 methodId][i16 spaceId][i16 flag]
//            + 两个 mfc_string(code/comment)；再跟参数流 0x36…0x01
//   - line_offset = 每个「显示行」行首在 expression 里的**绝对字节偏移**（不是每行字节数）
//   - 0xFF FF 不是空操作数：它是 `18 FF FF` = 逻辑 -1 = 真
#ifndef __EC_EEXPR_H
#define __EC_EEXPR_H

#include <afxwin.h>
#include <vector>
#include <string>

// 把一段 expression 解成若干「显示行」文本（已经是 eclone 的源码行格式，不含首尾换行）。
// pExpr/pLen  : expression 字节
// pLineOff/pLineOffLen : line_offset（u32 数组的原始字节）
// libIdx      : 该工程所用支持库在 ec_lib 里的下标（用于把 (spaceId,methodId) 解成真命令名）；
//               传 -1 时输出 `命令<lib,idx>` 占位
// 返回：行文本数组；失败时返回空数组并把 err 写进 strErr
std::vector<CString> EcDecodeExpression(const unsigned char* pExpr, size_t nExpr,
                                       const unsigned char* pLineOff, size_t nLineOff,
                                       int libIdx, CString& strErr);

#endif // __EC_EEXPR_H
