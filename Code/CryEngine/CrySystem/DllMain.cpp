// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   dllmain.cpp
//  Version:     v1.00
//  Created:     1/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "System.h"
#include <CryCore/Platform/platform_impl.inl>
#include "DebugCallStack.h"

#if CRY_PLATFORM_DURANGO
	#include "DurangoDebugCallstack.h"
#endif

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	#include <CrySystem/Scaleform/IScaleformHelper.h>
#endif

// For lua debugger
//#include <malloc.h>

HMODULE gDLLHandle = NULL;

struct DummyInitializer
{
	DummyInitializer()
	{
		dummyValue = 1;
	}

	int dummyValue;
};

DummyInitializer& initDummy()
{
	static DummyInitializer* p = new DummyInitializer;
	return *p;
}

static int warmAllocator = initDummy().dummyValue;

#if !defined(_LIB) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#pragma warning( push )
	#pragma warning( disable : 4447 )
BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
                      )
{
	gDLLHandle = (HMODULE)hModule;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:

		break;
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	//	int sbh = _set_sbh_threshold(1016);

	return TRUE;
}
	#pragma warning( pop )
#endif

#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	#include <CryMemory/CryMemoryAllocator.h>
	#include <CryMemory/BucketAllocator.h>
extern void EnableDynamicBucketCleanups(bool enable);
#endif

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_System : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			EnableDynamicBucketCleanups(true);
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			EnableDynamicBucketCleanups(false);
			break;
		}
#endif

		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			gEnv->pSystem->SetThreadState(ESubsys_Physics, false);
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_START:
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			{
#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
				if (!gEnv->IsDedicated() && gEnv->pScaleformHelper)
				{
					gEnv->pScaleformHelper->ResetMeshCache();
				}
#endif
				CryCleanup();
				break;
			}

		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				CryCleanup();
				gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
				break;
			}
		}
	}
};

static CSystemEventListner_System g_system_event_listener_system;

extern "C"
{
	CRYSYSTEM_API ISystem* CreateSystemInterface(const SSystemInitParams& startupParams)
	{
		CSystem* pSystem = NULL;

		pSystem = new CSystem(startupParams);
		ModuleInitISystem(pSystem, "CrySystem");
#if CRY_PLATFORM_DURANGO
	#if !defined(_LIB)
		gEnv = pSystem->GetGlobalEnvironment();
	#endif
		gEnv->pWindow = startupParams.hWnd;
#endif

		ICryFactoryRegistryImpl* pCryFactoryImpl = static_cast<ICryFactoryRegistryImpl*>(pSystem->GetCryFactoryRegistry());
		pCryFactoryImpl->RegisterFactories(g_pHeadToRegFactories);

		// the earliest point the system exists - w2e tell the callback
		if (startupParams.pUserCallback)
			startupParams.pUserCallback->OnSystemConnect(pSystem);

#if CRY_PLATFORM_WINDOWS
		// Install exception handler in Release modes.
		((DebugCallStack*)IDebugCallStack::instance())->installErrorHandler(pSystem);
#elif CRY_PLATFORM_DURANGO && defined(ENABLE_PROFILING_CODE)
		DurangoDebugCallStack::InstallExceptionHandler();
#endif
		if (!pSystem->Init())
		{
			delete pSystem;

			return 0;
		}

		pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE, 0, 0);
		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_system);

		return pSystem;
	}

	CRYSYSTEM_API void WINAPI CryInstallUnhandledExceptionHandler()
	{
#if CRY_PLATFORM_DURANGO && defined(ENABLE_PROFILING_CODE)
		DurangoDebugCallStack::InstallExceptionHandler();
#endif
	}

#if defined(ENABLE_PROFILING_CODE) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
	CRYSYSTEM_API void CryInstallPostExceptionHandler(void (* PostExceptionHandlerCallback)())
	{
		return IDebugCallStack::instance()->FileCreationCallback(PostExceptionHandlerCallback);
	}
#endif

};
