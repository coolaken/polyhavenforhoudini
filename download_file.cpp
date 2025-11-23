#include "download_file.h"

// --- 1. 通用回调 (核心技巧) ---
// 无论是存文件还是存内存，都把 stream 强转为 QIODevice
static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* stream) {
    QIODevice* device = static_cast<QIODevice*>(stream);
    if (device && device->isWritable()) {
        return device->write(static_cast<const char*>(ptr), size * nmemb);
    }
    return 0;
}

// --- 内部通用配置函数 (减少重复代码) ---
void setup_curl_common(CURL* curl, const char* url, QIODevice* device) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, device); // 传入 QFile* 或 QBuffer*

    // 公共配置
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Houdini-Plugin/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

// --- 2. GET: 获取小数据 (JSON/文本) ---
QByteArray get(const QUrl& url) {
    CURL* curl = curl_easy_init();
    QByteArray data;

    if (curl) {
        // 使用 QBuffer 在内存中读写数据
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly); // 必须打开

        // 生命周期管理
        QByteArray urlBytes = url.toEncoded();

        setup_curl_common(curl, urlBytes.constData(), &buffer);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            qWarning() << "GET Error:" << curl_easy_strerror(res);
            data.clear();
        }

        curl_easy_cleanup(curl);
        buffer.close();
    }
    return data;
}

// --- 3. DOWNLOAD: 下载大文件 (流式写入硬盘) ---
bool download_file(const QUrl& url, const QString& dest) {
    CURL* curl = curl_easy_init();
    bool success = false;

    if (curl) {
        QFile file(dest);
        if (file.open(QIODevice::WriteOnly)) {

            QByteArray urlBytes = url.toEncoded();

            setup_curl_common(curl, urlBytes.constData(), &file);

            CURLcode res = curl_easy_perform(curl);

            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

            if (res == CURLE_OK && http_code == 200) {
                qInfo() << "下载成功";
            }
            else {
                qWarning() << "下载失败，HTTP代码:" << http_code;
                file.close();
                file.remove(); // 删掉这个无效的文件，防止  报错
                return false;
            }
        }
        else {
            qCritical() << "Cannot open file:" << dest;
        }
        curl_easy_cleanup(curl);
    }
    return success;
}