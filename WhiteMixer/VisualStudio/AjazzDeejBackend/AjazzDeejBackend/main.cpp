#pragma comment(lib, "ws2_32.lib")
#include "json.hpp"
#include "easywsclient.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <vector>
#include <string>
#include <sstream>
#include <map>

using json = nlohmann::json;
using easywsclient::WebSocket;

// --- DICHIARAZIONI ---
std::wstring GetProcessNameFromID(DWORD processID);

struct ContextConfig {
    std::vector<std::wstring> apps;
    float sensitivity = 0.05f;
    bool dynamicSteps = false;
};

std::map<std::string, ContextConfig> contextSettings;

// --- UTILS ---
std::wstring StringToWString(const std::string& s) {
    if (s.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::vector<std::wstring> ParseAppList(std::string inputApps) {
    std::vector<std::wstring> appList;
    std::stringstream ss(inputApps);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t first = item.find_first_not_of(' ');
        if (std::string::npos == first) continue;
        size_t last = item.find_last_not_of(' ');
        std::string cleanItem = item.substr(first, (last - first + 1));
        if (!cleanItem.empty()) appList.push_back(StringToWString(cleanItem));
    }
    return appList;
}

// --- AUDIO CORE ---
void AdjustVolumeForList(const std::vector<std::wstring>& targetApps, int ticks, bool useDynamic, float staticSens) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    IMMDeviceEnumerator* deviceEnumerator = NULL;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);

    if (deviceEnumerator) {
        IMMDevice* defaultDevice = NULL;
        deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
        if (defaultDevice) {
            IAudioSessionManager2* sessionManager = NULL;
            defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&sessionManager);
            if (sessionManager) {
                IAudioSessionEnumerator* sessionEnumerator = NULL;
                sessionManager->GetSessionEnumerator(&sessionEnumerator);
                if (sessionEnumerator) {
                    int count;
                    sessionEnumerator->GetCount(&count);
                    for (int i = 0; i < count; i++) {
                        IAudioSessionControl* ctrl = NULL;
                        IAudioSessionControl2* ctrl2 = NULL;
                        sessionEnumerator->GetSession(i, &ctrl);
                        if (ctrl) {
                            ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2);
                            if (ctrl2) {
                                DWORD pid;
                                ctrl2->GetProcessId(&pid);
                                std::wstring currentProcName = GetProcessNameFromID(pid);
                                for (const auto& target : targetApps) {
                                    if (currentProcName.length() > 0 && _wcsicmp(currentProcName.c_str(), target.c_str()) == 0) {
                                        ISimpleAudioVolume* vol = NULL;
                                        ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol);
                                        if (vol) {
                                            float currentVol;
                                            vol->GetMasterVolume(&currentVol);

                                            // --- LOGICA DIREZIONALE PERFETTA ---
                                            float step = staticSens; // Default

                                            if (useDynamic) {
                                                float volPercent = currentVol * 100.0f;
                                                bool goingUp = ticks > 0; // Stiamo alzando il volume?

                                                if (goingUp) {
                                                    // SALITA: I bordi scattano "dopo" il numero
                                                    // 0-11 -> Step 1
                                                    if (volPercent < 11.5f) step = 0.01f;
                                                    // 12-39 -> Step 2 (Così a 38+2 fa 40)
                                                    else if (volPercent < 39.5f) step = 0.02f;
                                                    // 40+ -> Step 5 (Così a 40+5 fa 45)
                                                    else step = 0.05f;
                                                }
                                                else {
                                                    // DISCESA: I bordi scattano "sul" numero
                                                    // Se siamo a 41+, scendi veloce. A 40 scendi piano.
                                                    if (volPercent > 40.5f) step = 0.05f;
                                                    // Se siamo a 11+, scendi medio. A 10 scendi piano.
                                                    else if (volPercent > 10.5f) step = 0.02f;
                                                    // Sotto il 10 scendi piano
                                                    else step = 0.01f;
                                                }
                                            }

                                            float volChange = ticks * step;
                                            float newVol = currentVol + volChange;

                                            if (newVol > 1.0f) newVol = 1.0f;
                                            if (newVol < 0.0f) newVol = 0.0f;

                                            vol->SetMasterVolume(newVol, NULL);
                                            vol->SetMute(FALSE, NULL);
                                            vol->Release();
                                        }
                                    }
                                }
                                ctrl2->Release();
                            }
                            ctrl->Release();
                        }
                    }
                    sessionEnumerator->Release();
                }
                sessionManager->Release();
            }
            defaultDevice->Release();
        }
        deviceEnumerator->Release();
    }
    CoUninitialize();
}

void ToggleMuteForList(const std::vector<std::wstring>& targetApps) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    IMMDeviceEnumerator* deviceEnumerator = NULL;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);

    if (deviceEnumerator) {
        IMMDevice* defaultDevice = NULL;
        deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
        if (defaultDevice) {
            IAudioSessionManager2* sessionManager = NULL;
            defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&sessionManager);
            if (sessionManager) {
                IAudioSessionEnumerator* sessionEnumerator = NULL;
                sessionManager->GetSessionEnumerator(&sessionEnumerator);
                if (sessionEnumerator) {
                    int count;
                    sessionEnumerator->GetCount(&count);
                    for (int i = 0; i < count; i++) {
                        IAudioSessionControl* ctrl = NULL;
                        IAudioSessionControl2* ctrl2 = NULL;
                        sessionEnumerator->GetSession(i, &ctrl);
                        if (ctrl) {
                            ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2);
                            if (ctrl2) {
                                DWORD pid;
                                ctrl2->GetProcessId(&pid);
                                std::wstring currentProcName = GetProcessNameFromID(pid);
                                for (const auto& target : targetApps) {
                                    if (currentProcName.length() > 0 && _wcsicmp(currentProcName.c_str(), target.c_str()) == 0) {
                                        ISimpleAudioVolume* vol = NULL;
                                        ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol);
                                        if (vol) {
                                            BOOL isMuted;
                                            vol->GetMute(&isMuted);
                                            vol->SetMute(!isMuted, NULL);
                                            vol->Release();
                                        }
                                    }
                                }
                                ctrl2->Release();
                            }
                            ctrl->Release();
                        }
                    }
                    sessionEnumerator->Release();
                }
                sessionManager->Release();
            }
            defaultDevice->Release();
        }
        deviceEnumerator->Release();
    }
    CoUninitialize();
}

// --- MAIN ---

int main(int argc, const char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    std::string port = "", pluginUUID = "", registerEvent = "";
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-port") port = argv[i + 1];
        else if (arg == "-pluginUUID") pluginUUID = argv[i + 1];
        else if (arg == "-registerEvent") registerEvent = argv[i + 1];
    }

    if (port.empty()) return 0;

    std::string uri = "ws://127.0.0.1:" + port;
    std::unique_ptr<WebSocket> ws(WebSocket::from_url(uri));

    if (!ws) return 1;

    json registration;
    registration["event"] = registerEvent;
    registration["uuid"] = pluginUUID;
    ws->send(registration.dump());

    while (ws->getReadyState() != WebSocket::CLOSED) {
        ws->poll();

        ws->dispatch([&](const std::string& message) {
            try {
                auto jsonMsg = json::parse(message);
                std::string event = jsonMsg.value("event", "");
                std::string context = jsonMsg.value("context", "");

                // SETTINGS
                if (event == "didReceiveSettings" || event == "willAppear") {
                    if (jsonMsg.contains("payload") && jsonMsg["payload"].contains("settings")) {
                        auto settings = jsonMsg["payload"]["settings"];
                        std::string apps = settings.value("appList", "chrome.exe");
                        float sensVal = 5.0f;
                        if (settings.contains("sensitivity")) {
                            if (settings["sensitivity"].is_string()) {
                                try { sensVal = std::stof(settings["sensitivity"].get<std::string>()); }
                                catch (...) {}
                            }
                            else {
                                sensVal = settings["sensitivity"];
                            }
                        }
                        bool dyn = false;
                        if (settings.contains("dynamicSteps")) {
                            dyn = settings["dynamicSteps"];
                        }
                        contextSettings[context].apps = ParseAppList(apps);
                        contextSettings[context].sensitivity = sensVal / 100.0f;
                        contextSettings[context].dynamicSteps = dyn;
                    }
                }

                // ROTAZIONE
                if (event == "dialRotate") {
                    int ticks = jsonMsg["payload"]["ticks"];
                    bool useDyn = false;
                    float staticSens = 0.05f;
                    std::vector<std::wstring> apps = { L"chrome.exe" };

                    if (contextSettings.count(context)) {
                        useDyn = contextSettings[context].dynamicSteps;
                        staticSens = contextSettings[context].sensitivity;
                        apps = contextSettings[context].apps;
                    }
                    AdjustVolumeForList(apps, ticks, useDyn, staticSens);
                }

                // CLICK
                bool actionTriggered = false;
                if (event == "dialPress") {
                    if (jsonMsg["payload"].value("pressed", false) == true) actionTriggered = true;
                }
                else if (event == "dialDown" || event == "keyUp" || event == "touchTap") {
                    actionTriggered = true;
                }

                if (actionTriggered) {
                    if (contextSettings.count(context)) ToggleMuteForList(contextSettings[context].apps);
                    else {
                        std::vector<std::wstring> def = { L"chrome.exe" };
                        ToggleMuteForList(def);
                    }
                }
            }
            catch (...) {}
            });

        Sleep(15);
    }

    WSACleanup();
    return 0;
}