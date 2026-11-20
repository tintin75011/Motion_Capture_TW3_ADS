///////////////////////////////////////////////////////////////////////////////
// ADS_COMDriver.h

#ifndef __ADS_COMDRIVER_H__
#define __ADS_COMDRIVER_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TcBase.h"

#define ADS_COMDRV_NAME        "ADS_COM"
#define ADS_COMDRV_Major       1
#define ADS_COMDRV_Minor       0

#define DEVICE_CLASS CADS_COMDriver

#include "ObjDriver.h"

class CADS_COMDriver : public CObjDriver
{
public:
	virtual IOSTATUS	OnLoad();
	virtual VOID		OnUnLoad();

	//////////////////////////////////////////////////////
	// VxD-Services exported by this driver
	static unsigned long	_cdecl ADS_COMDRV_GetVersion();
	//////////////////////////////////////////////////////
	
};

Begin_VxD_Service_Table(ADS_COMDRV)
	VxD_Service( ADS_COMDRV_GetVersion )
End_VxD_Service_Table


#endif // ifndef __ADS_COMDRIVER_H__