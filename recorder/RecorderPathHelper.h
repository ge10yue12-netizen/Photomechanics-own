#pragma once

#include "RecorderKit.h"

class QString;

// 录屏默认路径（对齐 AppLogger：优先 exe 目录下 Record/）。
namespace RecorderPathHelper
{

QString recordRootDir();

QString extensionFor(recorder::VideoFormat format);

QString defaultOutputFile(recorder::VideoFormat format);

bool ensureRecordRootExists(QString *errorMessage);

QString historyFilePath();

} // namespace RecorderPathHelper
