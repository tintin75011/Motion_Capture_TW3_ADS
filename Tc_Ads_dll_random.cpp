#include <iostream>
#include <windows.h>
#include "TcAdsDef.h"
#include "TcAdsAPI.h"
#include <cstdint>

using namespace std;

void printAmsAddr(const AmsAddr& addr);

int main()
{
   // étape 1 : ouvrir le port local
   long nPort, nErr = 0;
   nPort = AdsPortOpenEx();
   AmsAddr addr;
   PAmsAddr paddr = &addr;
   //étape 2 : Construire l'addresse de la cible
   nErr = AdsGetLocalAddressEx(nPort,paddr);
   addr.port = 350;
   printAmsAddr(addr);
   if(nErr != 0){
      cerr << "Erreur lors de l'ouverture du port Local : " << nErr << '\n' ; 
   }

   // étape 3 : obtenir les handleInputs des variables

   char nameInputSymbol[] = "Object1 (Module1).Inputs.Value";
   char nameOutputSymbol[] = "Object1 (Module1).Outputs.Value";

   unsigned long handleInput =0;
   unsigned long handleOutput =0;
   unsigned long nRead = 0;
   // get handleInput
   nErr = AdsSyncReadWriteReqEx2(nPort, paddr,
                           ADSIGRP_SYM_HNDBYNAME, 0,
                           sizeof(handleInput), &handleInput,
                           sizeof(nameInputSymbol), nameInputSymbol,
                           &nRead);
   if (nErr) { std::cerr << "nerr Input handle: " << nErr << '\n'; return 1; }
   
   //getOutputHandle
   nErr = AdsSyncReadWriteReqEx2(nPort, paddr,
                           ADSIGRP_SYM_HNDBYNAME, 0,
                           sizeof(handleOutput), &handleOutput,
                           sizeof(nameOutputSymbol), nameOutputSymbol,
                           &nRead);
   if (nErr) { std::cerr << "nerr Output handle: " << nErr << '\n'; return 1; }
   

   // étape 4 : écrire une valeur dans la variable 
   int32_t valueInput = 1;
   nErr = AdsSyncWriteReqEx(nPort, paddr,
                        ADSIGRP_SYM_VALBYHND,   // écriture par handleInput
                        handleInput,                 
                        sizeof(valueInput), &valueInput);
   if (nErr) { std::cerr << "Write: " << nErr << '\n'; return 1; }

   // Lire la variable 
   for (int i = 0; i < 5; ++i) {
       int32_t valueOutput = 0;
       nErr = AdsSyncReadReqEx2(nPort, paddr,
                           ADSIGRP_SYM_VALBYHND,   // lecture par handleOutput
                           handleOutput,
                           sizeof(valueOutput), &valueOutput,
                           &nRead);
       if (nErr) { std::cerr << "Read: " << nErr << '\n'; return 1; }
       std::cout << "Value read: " << valueOutput << std::endl;
       Sleep(1000); // Attendre 1 seconde avant la prochaine lecture
   }

   // étape 5 : libérer les handles
   AdsSyncWriteReqEx(nPort, &addr, ADSIGRP_SYM_RELEASEHND, 0,
                      sizeof(handleInput), &handleInput);
   AdsSyncWriteReqEx(nPort, &addr, ADSIGRP_SYM_RELEASEHND, 0,
                      sizeof(handleOutput), &handleOutput);

    // 6. Fermer le port
    AdsPortCloseEx(nPort);
    return 0;
}



void printAmsAddr(const AmsAddr& addr) {
    std::cout << "addresse AMS : ";
    for (int i = 0; i < 6; ++i) {
        std::cout << static_cast<int>(addr.netId.b[i]);
        if (i < 5) {
            std::cout << ".";
        }
    }
    std::cout << ":" << addr.port << std::endl;
}