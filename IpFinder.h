#pragma once

#include <string>
#include <vector>

// Informations sur une interface Ethernet IPv4
struct EthInterfaceInfo
{
    std::string   name;          // Nom convivial (ex: "Ethernet 2")
    std::string   description;   // Description de la carte réseau
    std::string   ipv4;          // Adresse IPv4 (ex: "192.168.1.20")
    unsigned long prefixLength;  // Longueur du préfixe (ex: 24)
};

class IpFinder
{
public:
    explicit IpFinder(std::string targetIp = "192.168.1.1",
                      unsigned long timeoutMs = 1000,
                      bool requireSameSubnet = true);

    // Liste les interfaces Ethernet actives disposant d'une adresse IPv4
    static std::vector<EthInterfaceInfo> listEthernetInterfaces();

    // Parcourt les interfaces Ethernet et ping la cible depuis chacune.
    // Retourne true si au moins une interface a réussi.
    bool findInterfaceReachingTarget();

    // Adresse IP locale de l'interface ayant réussi à ping (vide si aucune)
    const std::string& reachingIp() const { return m_reachingIp; }

    // Détails de l'interface ayant réussi
    const EthInterfaceInfo& reachingInterface() const { return m_reachingInterface; }

    const std::string& targetIp() const { return m_targetIp; }

private:
    // Ping la cible en forçant l'adresse source
    bool pingFrom(const std::string& sourceIp) const;

    // Vérifie que la cible est dans le sous-réseau de l'interface
    bool targetInSubnet(const EthInterfaceInfo& itf) const;

    std::string      m_targetIp;
    unsigned long    m_timeoutMs;
    bool             m_requireSameSubnet;
    std::string      m_reachingIp;
    EthInterfaceInfo m_reachingInterface{};
};