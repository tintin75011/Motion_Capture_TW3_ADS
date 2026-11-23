# Motion_Capture_TW3_ADS
Lire et écrire des variables dans TwinCAT3 depuis un script C++ avec la lib TcAdsDll. 

## Prérequis

### C++ (build de `Art_to_ads.exe`)

- **CMake ≥ 3.25** (4.x recommandé)
- **Ninja** + **MinGW-w64 (g++)** — configuration par défaut
  ```powershell
  winget install Kitware.CMake
  winget install Ninja-build.Ninja
  winget install MSYS2.MSYS2   # puis : pacman -S mingw-w64-ucrt-x86_64-gcc
  ```
- *Optionnel* : **Visual Studio 2022** (ou Build Tools) avec la charge "Développement Desktop en C++" pour les presets `vs2022-*`
- **git** (pour le submodule)


## Installation
Localiser l'emplacement des fichiers  `TcAdsDef.h` et` TcAdsAPI.h `et modifier la ligne 18 de votre `CMakeLists.txt` en conséquence:

## Build (C++)

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

```powershell
.\build\Tc_ads_dll_random.exe
```
