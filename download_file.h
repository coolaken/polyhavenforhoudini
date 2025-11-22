#pragma once

#include <curl/curl.h>
#include <QtCore/qfile.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qurl.h>
#include <QtCore/qdebug.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qstring.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qiodevice.h>
//#include <stdio.h>
//#include <iostream>

static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* stream);

void setup_curl_common(CURL* curl, const char* url, QIODevice* device);

QByteArray get(const QUrl& url);

bool download_file(const QUrl& url, const QString& dest);

