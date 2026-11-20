///////////////////////////////////////////////////////////////////////////////
// ADS_COMDriver.cpp
#include "TcPch.h"
#pragma hdrstop

#include "ADS_COMDriver.h"
#include "ADS_COMClassFactory.h"

DECLARE_GENERIC_DEVICE(ADS_COMDRV)

IOSTATUS CADS_COMDriver::OnLoad( )
{
	TRACE(_T("CObjClassFactory::OnLoad()\n") );
	m_pObjClassFactory = new CADS_COMClassFactory();

	return IOSTATUS_SUCCESS;
}

VOID CADS_COMDriver::OnUnLoad( )
{
	delete m_pObjClassFactory;
}

unsigned long _cdecl CADS_COMDriver::ADS_COMDRV_GetVersion( )
{
	return( (ADS_COMDRV_Major << 8) | ADS_COMDRV_Minor );
}

