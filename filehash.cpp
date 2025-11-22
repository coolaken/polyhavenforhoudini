#include "filehash.h"
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>

// 分块大小：与 Python 一致（8192 字节），平衡性能和内存占用
static const qint64 CHUNK_SIZE = 8192;

QString filehash(const QString& filePath) {
    // 打开文件（只读 + 二进制模式，与 Python 的 "rb" 一致）
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {  // Unbuffered 减少额外内存开销
        qWarning() << "[filehash] Failed to open file for reading:" << filePath
            << "Error:" << file.errorString();
        return "";
    }

    // 初始化 MD5 哈希计算器（Qt 原生支持，与 Python hashlib.md5 一致）
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.reset();  // 确保初始状态干净

    // 分块读取文件并更新哈希（内存高效）
    QByteArray chunk;
    while (!(chunk = file.read(CHUNK_SIZE)).isEmpty()) {
        hash.addData(chunk);  // 每次添加 8192 字节的块
    }

    // 检查读取过程中是否出错
    if (file.error() != QFile::NoError) {
        qWarning() << "[filehash] Error reading file:" << filePath
            << "Error:" << file.errorString();
        file.close();
        return "";
    }

    // 关闭文件
    file.close();

    // 生成哈希字符串（小写，与 Python 的 hexdigest() 一致）
    return hash.result().toHex().toLower();
}