#include "IpFinder.h"

// L'ordre des includes est important sous Windows
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <cstring>
#include <utility>

IpFinder::IpFinder(std::string targetIp, unsigned long timeoutMs, bool requireSameSubnet)
    : m_targetIp(std::move(targetIp)),
      m_timeoutMs(timeoutMs),
      m_requireSameSubnet(requireSameSubnet)
{
}

std::vector<EthInterfaceInfo> IpFinder::listEthernetInterfaces()
{
    std::vector<EthInterfaceInfo> result;

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG bufLen = 15000;
    std::vector<unsigned char> buffer;

    ULONG ret = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && ret == ERROR_BUFFER_OVERFLOW; ++attempt)
    {
        buffer.resize(bufLen);
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr,
                                   reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()),
                                   &bufLen);
    }
    if (ret != NO_ERROR)
        return result;

    auto* adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    for (; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->IfType != IF_TYPE_ETHERNET_CSMACD) continue;   // Ethernet uniquement
        if (adapter->OperStatus != IfOperStatusUp)      continue;   // interface active

        // Conversion des noms (wide -> UTF-8)
        auto toUtf8 = [](const wchar_t* w) -> std::string {
            if (!w) return {};
            int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 1) return {};
            std::string s(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
            return s;
        };

        for (auto* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next)
        {
            if (!ua->Address.lpSockaddr || ua->Address.lpSockaddr->sa_family != AF_INET)
                continue;

            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));

            EthInterfaceInfo info;
            info.name         = toUtf8(adapter->FriendlyName);
            info.description  = toUtf8(adapter->Description);
            info.ipv4         = ip;
            info.prefixLength = ua->OnLinkPrefixLength;
            result.push_back(std::move(info));
        }
    }
    return result;
}

bool IpFinder::targetInSubnet(const EthInterfaceInfo& itf) const
{
    in_addr local{}, target{};
    if (inet_pton(AF_INET, itf.ipv4.c_str(), &local) != 1) return false;
    if (inet_pton(AF_INET, m_targetIp.c_str(), &target) != 1) return false;

    unsigned long prefix = itf.prefixLength;
    if (prefix > 32) return false;

    // Masque en ordre réseau
    unsigned long maskHost = (prefix == 0) ? 0UL : (0xFFFFFFFFUL << (32 - prefix));
    unsigned long mask     = htonl(maskHost);

    return (local.S_un.S_addr & mask) == (target.S_un.S_addr & mask);
}

bool IpFinder::pingFrom(const std::string& sourceIp) const
{
    in_addr src{}, dst{};
    if (inet_pton(AF_INET, sourceIp.c_str(), &src) != 1) return false;
    if (inet_pton(AF_INET, m_targetIp.c_str(), &dst) != 1) return false;

    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return false;

    char sendData[32] = "IpFinder ping";
    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::vector<unsigned char> replyBuf(replySize);

    // Version synchrone avec adresse source imposée
    DWORD count = IcmpSendEcho2Ex(hIcmp,
                                  nullptr, nullptr, nullptr,   // pas d'event/APC
                                  src.S_un.S_addr,             // adresse source
                                  dst.S_un.S_addr,             // adresse destination
                                  sendData, sizeof(sendData),
                                  nullptr,                     // options IP par défaut
                                  replyBuf.data(), replySize,
                                  m_timeoutMs);

    bool ok = false;
    if (count > 0)
    {
        auto* reply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuf.data());
        ok = (reply->Status == IP_SUCCESS);
    }

    IcmpCloseHandle(hIcmp);
    return ok;
}

bool IpFinder::findInterfaceReachingTarget()
{
    m_reachingIp.clear();
    m_reachingInterface = EthInterfaceInfo{};

    // Initialisation Winsock (nécessaire pour inet_pton/htonl selon contexte)
    WSADATA wsa;
    const bool wsaOk = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);

    bool found = false;
    for (const auto& itf : listEthernetInterfaces())
    {
        if (m_requireSameSubnet && !targetInSubnet(itf))
            continue;

        if (pingFrom(itf.ipv4))
        {
            m_reachingIp        = itf.ipv4;
            m_reachingInterface = itf;
            found = true;
            break;
        }
    }

    if (wsaOk) WSACleanup();
    return found;
}