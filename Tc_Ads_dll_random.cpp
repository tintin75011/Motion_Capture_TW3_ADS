#include <iostream>
#include <conio.h>
#include <windows.h>

#include "TcAdsDef.h"
#include "TcAdsAPI.h"

int main()                     
{
    long nTemp = AdsGetDllVersion();
    AdsVersion* pDLLVersion = (AdsVersion*)&nTemp;
    std::cout << "Version: "  << (int)pDLLVersion->version  << '\n';
    std::cout << "Revision: " << (int)pDLLVersion->revision << '\n';
    std::cout << "Build: "    << pDLLVersion->build         << '\n';
    std::cout.flush();

    _getch();                   
    return 0;
}