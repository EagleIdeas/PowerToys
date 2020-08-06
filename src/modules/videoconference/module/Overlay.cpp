#include "pch.h"
#include "Overlay.h"

#include <windowsx.h>

#include "VideoConferenceModule.h"

Gdiplus::Image* Overlay::camOnMicOnBitmap = nullptr;
Gdiplus::Image* Overlay::camOffMicOnBitmap = nullptr;
Gdiplus::Image* Overlay::camOnMicOffBitmap = nullptr;
Gdiplus::Image* Overlay::camOffMicOffBitmap = nullptr;

bool Overlay::valueUpdated = false;
bool Overlay::cameraMuted = false;
bool Overlay::microphoneMuted = false;
bool Overlay::hideOverlayWhenUnmuted = true;

std::vector<HWND> Overlay::hwnds;

UINT_PTR Overlay::nTimerId;

unsigned __int64 Overlay::lastTimeCamOrMicMuteStateChanged;

const int REFRESH_RATE = 100;
const int OVERLAY_SHOW_TIME = 500;
const int BORDER_OFFSET = 12;

Overlay::Overlay()
{
    camOnMicOnBitmap = Gdiplus::Image::FromFile(L"modules/VideoConference/On-On Dark.png");
    camOffMicOnBitmap = Gdiplus::Image::FromFile(L"modules/VideoConference/On-Off Dark.png");
    camOnMicOffBitmap = Gdiplus::Image::FromFile(L"modules/VideoConference/Off-On Dark.png");
    camOffMicOffBitmap = Gdiplus::Image::FromFile(L"modules/VideoConference/Off-Off Dark.png");
}

LRESULT Overlay::WindowProcessMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        return 0;
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);

        if (x < 322 / 2)
        {
            VideoConferenceModule::reverseMicrophoneMute();
        }
        else
        {
            VideoConferenceModule::reverseVirtualCameraMuteState();
            setCameraMute(VideoConferenceModule::getVirtualCameraMuteState());
        }

        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    case WM_CREATE:
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;

        hdc = BeginPaint(hwnd, &ps);

        Gdiplus::Graphics graphic(hdc);

        if (microphoneMuted)
        {
            if (cameraMuted)
            {
                graphic.DrawImage(camOffMicOffBitmap, 0, 0, camOffMicOffBitmap->GetWidth(), camOffMicOffBitmap->GetHeight());
            }
            else
            {
                graphic.DrawImage(camOnMicOffBitmap, 0, 0, camOnMicOffBitmap->GetWidth(), camOnMicOffBitmap->GetHeight());
            }
        }
        else
        {
            if (cameraMuted)
            {
                graphic.DrawImage(camOffMicOnBitmap, 0, 0, camOffMicOnBitmap->GetWidth(), camOffMicOnBitmap->GetHeight());
            }
            else
            {
                graphic.DrawImage(camOnMicOnBitmap, 0, 0, camOnMicOnBitmap->GetWidth(), camOnMicOnBitmap->GetHeight());
            }
        }

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_TIMER:
    {
        InvalidateRect(hwnd, NULL, NULL);
        using namespace std::chrono;
        
        if (cameraMuted || microphoneMuted || !hideOverlayWhenUnmuted)
        {
            ShowWindow(hwnd, SW_SHOW);
        }
        else
        {
            if (duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - lastTimeCamOrMicMuteStateChanged > OVERLAY_SHOW_TIME || !valueUpdated)
            {
                ShowWindow(hwnd, SW_HIDE);
            }
            else
            {
                ShowWindow(hwnd, SW_SHOW);
            }
        }

        KillTimer(hwnd, nTimerId);

        break;
    }
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    nTimerId = SetTimer(hwnd, 101, REFRESH_RATE, NULL);

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Overlay::showOverlay(std::wstring position, std::wstring monitorString)
{
    valueUpdated = false;
    for (auto& hwnd : hwnds)
    {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    hwnds.clear();

    int overlayWidth = camOffMicOffBitmap->GetWidth();
    int overlayHeight = camOffMicOffBitmap->GetHeight();

    // Register the window class
    LPCWSTR CLASS_NAME = L"MuteNotificationWindowClass";
    WNDCLASS wc{};
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpfnWndProc = WindowProcessMessages;
    RegisterClass(&wc);

    // Create the window
    DWORD dwExtStyle = 0;
    DWORD dwStyle = WS_POPUPWINDOW;

    std::vector<MonitorInfo> monitorInfos;


    if (monitorString == L"All monitors")
    {
        monitorInfos = MonitorInfo::GetMonitors(false);
    }
    else //"Main monitor" or non-present
    {
        monitorInfos.push_back(MonitorInfo::GetPrimaryMonitor());
    }

    for (auto& monitorInfo : monitorInfos)
    {
        int positionX = 0;
        int positionY = 0;

        if (position == L"Top left corner")
        {
            positionX = monitorInfo.left() + BORDER_OFFSET;
            positionY = monitorInfo.top() + BORDER_OFFSET;
        }
        else if (position == L"Center")
        {
            positionX = monitorInfo.middle().x - overlayWidth / 2;
            positionY = monitorInfo.middle().y - overlayHeight / 2;
        }
        else if (position == L"Bottom left corner")
        {
            positionX = monitorInfo.left() + BORDER_OFFSET;
            positionY = monitorInfo.bottom() - overlayHeight - BORDER_OFFSET;
        }
        else if (position == L"Bottom right corner")
        {
            positionX = monitorInfo.right() - overlayWidth - BORDER_OFFSET;
            positionY = monitorInfo.bottom() - overlayHeight - BORDER_OFFSET;
        }
        else //"Top right corner" or non-present
        {
            positionX = monitorInfo.right() - overlayWidth - BORDER_OFFSET;
            positionY = monitorInfo.top() + BORDER_OFFSET;
        }

        HWND hwnd;
        hwnd = CreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CLASS_NAME,
            CLASS_NAME,
            WS_POPUP,
            positionX,
            positionY,
            overlayWidth,
            overlayHeight,
            NULL,
            NULL,
            GetModuleHandle(NULL),
            NULL);

        auto transparrentColorKey = RGB(0, 0, 255);
        HBRUSH brush = CreateSolidBrush(transparrentColorKey);
        SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);

        SetLayeredWindowAttributes(hwnd, transparrentColorKey, 0, LWA_COLORKEY);

        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        hwnds.push_back(hwnd);
    }
}

void Overlay::hideOverlay()
{
    for (auto& hwnd : hwnds)
    {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    hwnds.clear();
}

bool Overlay::getCameraMute()
{
    return cameraMuted;
}

void Overlay::setCameraMute(bool mute)
{
    valueUpdated = true;
    lastTimeCamOrMicMuteStateChanged = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    cameraMuted = mute;
}

bool Overlay::getMicrophoneMute()
{
    return microphoneMuted;
}

void Overlay::setMicrophoneMute(bool mute)
{
    if (mute != microphoneMuted)
    {
        valueUpdated = true;
        lastTimeCamOrMicMuteStateChanged = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    microphoneMuted = mute;
}

void Overlay::setHideOverlayWhenUnmuted(bool hide)
{
    hideOverlayWhenUnmuted = hide;
}
