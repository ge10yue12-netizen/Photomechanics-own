#include "RecorderVideoProbe.h"

#include <QFileInfo>
#include <QUrl>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <oleauto.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace RecorderVideoProbe
{
namespace
{

int durationFromPropVariant(const PROPVARIANT &var)
{
    if (var.vt != VT_UI8 || var.uhVal.QuadPart <= 0)
        return 0;
    return hundredNanosecondsToRoundedSeconds(var.uhVal.QuadPart);
}

int probeWithMediaFoundation(const QString &absoluteFilePath)
{
    const QString url = QUrl::fromLocalFile(QFileInfo(absoluteFilePath).absoluteFilePath())
                            .toString(QUrl::FullyEncoded);
    const std::wstring wurl = url.toStdWString();

    IMFSourceReader *reader = nullptr;
    const HRESULT createHr = MFCreateSourceReaderFromURL(wurl.c_str(), nullptr, &reader);
    if (FAILED(createHr) || !reader)
        return 0;

    PROPVARIANT var;
    PropVariantInit(&var);
    const HRESULT attrHr =
        reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
    const int seconds = SUCCEEDED(attrHr) ? durationFromPropVariant(var) : 0;
    PropVariantClear(&var);
    reader->Release();
    return seconds;
}

} // namespace

int hundredNanosecondsToRoundedSeconds(std::uint64_t hundredNanoseconds)
{
    if (hundredNanoseconds <= 0)
        return 0;
    return static_cast<int>((hundredNanoseconds + 5000000ULL) / 10000000ULL);
}

int durationSeconds(const QString &absoluteFilePath)
{
    if (absoluteFilePath.isEmpty() || !QFileInfo::exists(absoluteFilePath))
        return 0;

    const HRESULT mfHr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(mfHr))
        return 0;

    const int seconds = probeWithMediaFoundation(absoluteFilePath);
    MFShutdown();
    return seconds > 0 ? seconds : 0;
}

int durationSecondsWithFallback(const QString &absoluteFilePath, int fallbackSeconds)
{
    const int probed = durationSeconds(absoluteFilePath);
    if (probed > 0)
        return probed;
    return fallbackSeconds < 0 ? 0 : fallbackSeconds;
}

} // namespace RecorderVideoProbe
