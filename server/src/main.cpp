//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <consoleapi.h>
#include <processthreadsapi.h>
#else
#include <signal.h>
#endif

#include <filesystem>
#include <thread>
#include <chrono>

#include "log_utils.hpp"

#include "core/ks_core.hpp"
#include "core/ks_cloudflare.hpp"
#include "core/ks_response.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::Cloudflare;

#ifdef _WIN32
using std::wstring;
#endif

using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::weakly_canonical;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
using std::string;

#ifdef _WIN32
BOOL WINAPI HandleClose(DWORD signal)
{
    if (signal == CTRL_C_EVENT
        || signal == CTRL_CLOSE_EVENT
        || signal == CTRL_BREAK_EVENT)
    {
        KalaServerCore::Shutdown();
        ExitProcess(0);

        return TRUE;
    }
    
    return FALSE;
}
#else
static void HandleClose(int)
{ 
    KalaServerCore::Shutdown();
    _exit(0);
}
#endif

int main()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(HandleClose, TRUE);
#else
    signal(SIGINT, HandleClose);
    signal(SIGTERM, HandleClose);
#endif

    path content = weakly_canonical(current_path() / "content");

    if (!exists(content))
    {
        Log::Print(
            "Failed to find content folder!",
            "WEBSITE_BACKEND",
            LogType::LOG_ERROR,
            2);

        exit(1);
    }

    Log::Print(
        "Found content folder '" + content.string() + "'",
        "WEBSITE_BACKEND",
        LogType::LOG_INFO);

    KalaServerCore::Initialize(
        "website_backend",
        content,
        "192.168.1.102",
        80,
        false);

    /*
    KalaServerCore::AddRoute( 
        {
            .domain = "thekalakit.com",
            .route = "/",
            .routePath = "."  
        });

    KalaServerCore::AddRoute( 
        {
            .domain = "elypsoengine.com",
            .route = "/112233",
            .routePath = "."  
        });

    KalaServerCore::AddBlacklistedKeyword("/wp-");
    KalaServerCore::AddBlacklistedKeyword("/user");
    KalaServerCore::AddBlacklistedKeyword("/login");
    KalaServerCore::AddBlacklistedKeyword("/admin");
    KalaServerCore::AddBlacklistedKeyword(".php");
    KalaServerCore::AddBlacklistedKeyword(".env");
    KalaServerCore::AddBlacklistedKeyword(".git");
    KalaServerCore::AddBlacklistedKeyword(".json");
    KalaServerCore::AddBlacklistedKeyword(".sql");
    KalaServerCore::AddBlacklistedKeyword(".sh");
    KalaServerCore::AddBlacklistedKeyword("bin");
    */

    if (KalaServerCore::IsCloudflareRequired())
    {
#ifdef _WIN32
        PWSTR winUserDir{};
        SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &winUserDir);

        path userDir = path(winUserDir) / ".cloudflared";
        CoTaskMemFree(winUserDir);

        path cloudflareExePath = current_path() / "cloudflared-windows-amd64.exe";
#else
        path userDir = path(getenv("HOME")) / ".cloudflared";
        path cloudflareExePath = current_path() / "cloudflared-linux-amd64";
#endif

        if (!Cloudflare::Initialize(
            "website_backend_tunnel", 
            cloudflareExePath, 
            userDir))
        {
            exit(1);
        }

        while (!KalaServerCore::IsReady())
        {
            sleep_for(milliseconds(1000));

            Log::Print(
                "Waiting for Cloudflare to finish connecting...",
                "WEBSITE_BACKEND",
                LogType::LOG_INFO);
        }
    }

    Log::Print(
        "Website backend is ready, starting accept loop!",
        "WEBSITE_BACKEND",
        LogType::LOG_INFO);

    while (true)
    { 
        KalaServerCore::Update();

        sleep_for(milliseconds(10));
    }
}