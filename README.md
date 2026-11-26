# Motion_Capture_TW3_ADS

Pont entre un système de tracking optique **ART / DTrack** et un runtime **TwinCAT 3** : le programme C++ reçoit en UDP la position et l'orientation d'un *body* DTrack, puis les écrit dans une variable TwinCAT via l'API **TcAdsDll** (ADS).

```
 Contrôleur DTrack ──UDP:5000──►  Art_to_ads.exe  ──ADS (port 350)──►  Runtime TwinCAT 3
   (192.168.1.1)                  (PC Windows)                         Object1 (Module1).Body_Loc.Body_1
```

## Sommaire

1. [Fonctionnement](#fonctionnement)
2. [Structure du projet et rôle des fichiers](#structure-du-projet-et-rôle-des-fichiers)
3. [Prérequis](#prérequis)
4. [Installation](#installation)
5. [Configuration](#configuration)
6. [Build](#build)
7. [Utilisation](#utilisation)
8. [Côté TwinCAT](#côté-twincat)
9. [Dépannage](#dépannage)

---

## Fonctionnement

Le programme utilise **3 threads** pour que la réception UDP ne soit jamais ralentie par l'écriture ADS ou par l'affichage :

| Thread | Fonction | Rôle |
|---|---|---|
| Réception DTrack | `artThread()` | Reçoit les paquets, filtre le body cible, convertit la matrice de rotation en angles d'Euler, publie le dernier échantillon. Aucun `print`, aucun appel ADS. |
| Écriture ADS | `adsThread()` | Attend un nouvel échantillon et écrit toute la structure (120 octets) en **une seule requête ADS**. Si ADS est lent, les échantillons intermédiaires sont sautés : c'est toujours le plus récent qui est écrit. |
| Principal | `displayLoop()` | Affiche l'état à 5 Hz (position, Euler, matrice, compteurs). |

Les threads communiquent via la structure `SharedState` (`g_state`) : un mutex + une condition variable protègent le dernier échantillon, et des compteurs atomiques servent aux statistiques.

**Séquence de démarrage** (`ART_To_ADS.cpp`) :

1. Initialisation Winsock et gestionnaires de signaux (`Ctrl+C` = arrêt propre)
2. `adsInit()` : ouverture du port ADS, récupération de l'adresse AMS, obtention du handle du symbole
3. `artInit()` : recherche de l'interface réseau locale qui joint le contrôleur (ping), configuration de la sortie UDP du contrôleur (`dtrack2 set output net ch01 udp <ip locale> <port>`), démarrage du tracking
4. Lancement des threads, puis boucle d'affichage jusqu'à l'arrêt
5. Arrêt propre : `join` des threads, arrêt du tracking, libération du handle ADS, fermeture du port

## Structure du projet et rôle des fichiers

```
Motion_Capture_TW3_ADS/
├── ART_To_ADS.cpp      # Point d'entrée (main)
├── config.h            # Paramètres modifiables
├── shared.h / .cpp     # Structure BodyData + état partagé entre threads
├── utils.h / .cpp      # Fonctions utilitaires (conversion Euler, noms de bodies)
├── ads.h / .cpp        # Communication TwinCAT ADS
├── art.h / .cpp        # Communication DTrack (ART)
├── IpFinder.h / .cpp   # Détection de l'interface réseau locale
├── CMakeLists.txt      # Script de build
├── CMakePresets.json   # Presets de configuration (Ninja/MinGW, VS 2022)
├── DTrackSDK/          # SDK DTrack (sous-module git)
├── TW3/                # Projet TwinCAT 3 (module C++ ADS_COM)
└── build/              # Sortie de compilation (générée)
```

### Code C++

| Fichier | Rôle |
|---|---|
| **`ART_To_ADS.cpp`** | Point d'entrée. Initialise Winsock, ADS et DTrack, lance les threads de réception et d'écriture, exécute la boucle d'affichage (`displayLoop`) et gère l'arrêt propre. Contient très peu de logique métier. |
| **`config.h`** | Regroupe tous les paramètres : IP du contrôleur DTrack, port UDP local, ID du body à transmettre, port ADS et nom du symbole TwinCAT. **C'est le seul fichier à modifier pour adapter le programme à une autre installation.** |
| **`shared.h` / `shared.cpp`** | Définit `BodyData` (structure de 15 `double` = 120 octets, échangée avec TwinCAT) et `SharedState` (mutex, condition variable, dernier échantillon, numéro de séquence, compteurs, flag d'arrêt). L'instance globale `g_state` est définie dans `shared.cpp`. |
| **`utils.h` / `utils.cpp`** | `rotToEulerDeg()` : convertit une matrice de rotation 3x3 (colonne par colonne) en angles d'Euler ZYX en degrés, avec gestion du gimbal lock. `bodyName()` : renvoie un nom lisible pour un ID de body. |
| **`ads.h` / `ads.cpp`** | Tout ce qui touche à TwinCAT : `AdsContext` (port, adresse AMS, handle), `adsInit()`, `adsRelease()` et `adsThread()` (écriture du dernier échantillon dans le symbole). |
| **`art.h` / `art.cpp`** | Tout ce qui touche à DTrack : `artInit()` (détection de l'interface, configuration du contrôleur, démarrage du tracking), `artShutdown()` et `artThread()` (réception, conversion et publication des données). |
| **`IpFinder.h` / `IpFinder.cpp`** | Classe `IpFinder` : liste les interfaces Ethernet IPv4 actives (`GetAdaptersAddresses`), vérifie que la cible est dans le même sous-réseau, puis envoie un ping ICMP en forçant l'adresse source (`IcmpSendEcho2Ex`). Permet de trouver automatiquement l'IP locale à communiquer au contrôleur DTrack comme destination UDP. |

### Build et dépendances

| Fichier / dossier | Rôle |
|---|---|
| **`CMakeLists.txt`** | Compile l'exécutable `Art_to_ads`, ajoute le sous-module `DTrackSDK` (bibliothèque statique), localise `TcAdsDll.lib` et lie `ws2_32`, `winmm` et `iphlpapi`. |
| **`CMakePresets.json`** | Presets de configuration/compilation (`ninja-release`, `ninja-debug`, `vs2022-release`, ...). |
| **`DTrackSDK/`** | SDK officiel ART pour recevoir et parser les données DTrack et piloter le contrôleur. |
| **`.gitmodules`** | Déclaration du sous-module `DTrackSDK`. |
| **`TW3/`** | Solution TwinCAT 3 (`TwinCAT ART_COM.sln`) contenant le module C++ `ADS_COM` qui expose le symbole `Body_Loc.Body_1` écrit par le programme. |

## Prérequis

- **Windows** (Winsock, iphlpapi et TcAdsDll sont spécifiques à Windows)
- **TwinCAT 3** installé (fournit `TcAdsDll`, `TcAdsDef.h` et `TcAdsAPI.h`)
- **CMake ≥ 3.18**
- **Ninja** + **MinGW-w64 (g++)** — configuration par défaut
  ```powershell
  winget install Kitware.CMake
  winget install Ninja-build.Ninja
  winget install MSYS2.MSYS2   # puis : pacman -S mingw-w64-ucrt-x86_64-gcc
  ```
- *Optionnel* : **Visual Studio 2022** (ou Build Tools) avec la charge « Développement Desktop en C++ » pour les presets `vs2022-*`
- **git** (pour le sous-module `DTrackSDK`)

## Installation

Cloner le dépôt **avec** son sous-module :

```powershell
git clone --recurse-submodules <url-du-depot>
```

Si le dépôt est déjà cloné :

```powershell
git submodule update --init --recursive
```

Localiser le dossier d'installation de TcAdsDll (celui qui contient `Include/TcAdsDef.h`, `Include/TcAdsAPI.h` et `Lib/x64/TcAdsDll.lib`). Par défaut :

```
C:/Program Files (x86)/Beckhoff/TwinCAT/AdsApi/TcAdsDll
```

Si votre installation est ailleurs, modifier la variable `TCADS_ROOT` dans `CMakeLists.txt`, ou la passer en ligne de commande :

```powershell
cmake --preset ninja-release -DTCADS_ROOT="D:/chemin/vers/TcAdsDll"
```

## Configuration

Tous les paramètres sont dans **`config.h`** :

| Paramètre | Valeur par défaut | Description |
|---|---|---|
| `CONTROLLER_IP` | `192.168.1.1` | Adresse IP du contrôleur DTrack |
| `DTRACK_PORT` | `5000` | Port UDP local de réception |
| `TARGET_BODY_ID` | `1` | ID du body à transmettre à TwinCAT |
| `ADS_PORT` | `350` | Port ADS du runtime hébergeant le symbole |
| `ADS_SYMBOL` | `Object1 (Module1).Body_Loc.Body_1` | Nom complet du symbole TwinCAT à écrire |

> Après modification de `config.h`, recompiler le projet.

### Structure échangée avec TwinCAT

La structure `BodyData` (`shared.h`) doit avoir **exactement la même disposition mémoire** que la `STRUCT` déclarée côté TwinCAT : 15 `LREAL` = 120 octets, sans padding.

| Champ | Type | Contenu |
|---|---|---|
| `loc[3]` | `LREAL` x3 | Position x, y, z (mm) |
| `euler[3]` | `LREAL` x3 | Roll, pitch, yaw (degrés), convention `R = Rz(yaw) · Ry(pitch) · Rx(roll)` |
| `rot[9]` | `LREAL` x9 | Matrice de rotation 3x3, colonne par colonne : `rot[col*3 + ligne]` |

## Build

Lister les configurations disponibles :

```powershell
cmake --list-presets
```

Configurer + compiler (Ninja / MinGW, Release) :

```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
```

Variante Debug :

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

Avec Visual Studio 2022 :

```powershell
cmake --preset vs2022-release
cmake --build --preset vs2022-release
```

## Utilisation

1. Démarrer le runtime TwinCAT (mode Run) avec le module `ADS_COM` chargé.
2. Vérifier que le PC et le contrôleur DTrack sont sur le même réseau.
3. Lancer le programme :

```powershell
.\build\Art_to_ads.exe
```

Le programme affiche d'abord l'adresse AMS et l'interface réseau trouvée, puis, à 5 Hz :

```
=== Body_1 ===  recus=...  ecrits=...  timeouts=...  ADS>10ms=...
Position [mm]        : x=...  y=...  z=...
Rotation Euler [deg] : roll=...  pitch=...  yaw=...
Matrice de rotation  : ...
```

| Compteur | Signification |
|---|---|
| `recus` | Échantillons du body cible reçus depuis DTrack |
| `ecrits` | Échantillons effectivement écrits dans TwinCAT (peut être inférieur à `recus` si ADS est lent) |
| `timeouts` | Timeouts de réception UDP |
| `ADS>10ms` | Nombre d'écritures ADS ayant pris plus de 10 ms |

Arrêt : `Ctrl+C` (arrêt propre : le tracking est stoppé et le handle ADS libéré).

## Côté TwinCAT

Le dossier `TW3/TwinCAT ART_COM/` contient la solution TwinCAT 3 avec le module C++ `ADS_COM` (`Module1`). Il expose la variable `Object1 (Module1).Body_Loc.Body_1` dont la structure doit correspondre à `BodyData`. Ouvrir `TwinCAT ART_COM.sln` dans Visual Studio / TwinCAT XAE, compiler puis activer la configuration.

## Dépannage

| Symptôme | Piste |
|---|---|
| `Erreur handle ADS` | Le runtime n'est pas en Run, le port ADS (`ADS_PORT`) est incorrect, ou le nom du symbole (`ADS_SYMBOL`) ne correspond pas exactement. |
| `Aucune interface locale ne peut atteindre ...` | Le contrôleur ne répond pas au ping depuis une interface Ethernet du PC : vérifier câble, IP du PC dans le même sous-réseau que `CONTROLLER_IP`, pare-feu (ICMP). |
| `Connexion au controleur impossible` | Le contrôleur est joignable mais l'interface de commande DTrack2 ne répond pas : vérifier l'IP et que le contrôleur est démarré. |
| `En attente de donnees` en boucle | Aucun paquet UDP reçu : vérifier que le pare-feu Windows autorise le port UDP `DTRACK_PORT`, que le body `TARGET_BODY_ID` est bien défini et visible par les caméras. |
| `[ADS] Write Error: <code>` | Le programme s'arrête. Consulter la documentation Beckhoff pour le code ADS ; vérifier notamment que la taille de la structure côté TwinCAT est bien de 120 octets. |
| `TcAdsDll.lib introuvable` | `TCADS_ROOT` ne pointe pas vers la bonne installation TwinCAT. |