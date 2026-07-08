#pragma once

#include "RecorderKit.h"

class QString;

// 录屏默认路径：项目根目录下 Record/（与 Log/ 同级；exe 在 x64/Debug 时上溯两级）。
namespace RecorderPathHelper
{

QString recordRootDir();

QString extensionFor(recorder::VideoFormat format);

QString defaultOutputFile(recorder::VideoFormat format);

// pathOrDir 可为目录或旧版完整文件路径；返回规范化的保存目录。
QString normalizeOutputDirectory(const QString &pathOrDir);

// 在指定目录下生成不重复的 record_yyyyMMdd_HHmmss 文件名（EV 同款：每次录制一个新文件）。
QString uniqueOutputFileInDir(const QString &outputDirectory, recorder::VideoFormat format);

bool ensureRecordRootExists(QString *errorMessage);

// 单元测试注入 Record 根目录；传空则恢复默认。
void setRecordRootDirOverride(const QString &absoluteDir);
void clearRecordRootDirOverride();

// 旧版 sidecar JSON 所在 Log/（仅用于一次性清理）。
QString legacyMetadataDirectory();

} // namespace RecorderPathHelper
