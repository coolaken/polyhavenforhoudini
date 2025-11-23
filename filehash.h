#ifndef FILE_HASH_H
#define FILE_HASH_H

#include <QtCore/QString>

/**
 * 计算文件的 MD5 哈希值（内存高效：分块读取，避免加载整个文件到内存）
 * 对应 Python: filehash(fp)
 * @param filePath 文件路径（绝对路径或相对路径）
 * @return MD5 哈希字符串（小写，32 位）；文件打开失败返回空字符串
 */
QString filehash(const QString& filePath);

#endif // FILE_HASH_H