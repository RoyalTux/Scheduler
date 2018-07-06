#include <Windows.h>
#include <tchar.h>
#include "resource.h"
#include <vector>
using namespace std;

struct Task
{
	TCHAR cmd[64];
	struct TimeMetric
	{
		int hours;
		int minutes;
		int seconds;
	}Time;
	BOOL isCompleted;
	HANDLE hTaskThread , hTimer;
};
typedef vector<Task>::iterator TaskIterator;

HWND hMainWnd;
vector<Task*> Tasks;
BOOL AddFlag;
HICON hIcon;
PNOTIFYICONDATA pNID;

//функции
DWORD WINAPI TaskThread(LPVOID);
void CheckButtons();
void EnableTaskEdits();
void DisableTaskEdits();
void UpdateList();

BOOL CALLBACK DlgMainProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgMainProc);
}

BOOL CALLBACK DlgMainProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
	case WM_INITDIALOG:
		hMainWnd = hWnd;

		hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON2));
		SetClassLong(hWnd, GCL_HICON, LONG(hIcon));

		pNID = new NOTIFYICONDATA;
		memset(pNID, 0, sizeof(NOTIFYICONDATA));
		pNID->cbSize = sizeof(NOTIFYICONDATA);
		//pNID->hIcon = nullptr;//hIcon;
		pNID->hIcon = hIcon;
		pNID->hWnd = hWnd;
		lstrcpy(pNID->szTip, TEXT("Планировщик задач"));
		pNID->uCallbackMessage = WM_APP;
		pNID->uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_INFO;
		lstrcpy(pNID->szInfo, TEXT("Спрятался"));
		lstrcpy(pNID->szInfoTitle, TEXT("Планировщик задач"));
		pNID->uID = WM_USER;
		return TRUE;

	case WM_CLOSE:
		// закрываем дескрипторы в векторе
		for(int i = 0; i < Tasks.size(); i++)
		{
			CloseHandle(Tasks[i]->hTaskThread);
			delete Tasks[i];
		}
		EndDialog(hWnd, 0);
		return TRUE;
	case WM_APP:
		{
			if(lParam == WM_LBUTTONDOWN)
			{
				Shell_NotifyIcon(NIM_DELETE, pNID); 
				ShowWindow(hWnd, SW_NORMAL);
				SetForegroundWindow(hWnd);
			}
			return TRUE;
		}
	case WM_SIZE:
		if(LOWORD(wParam) == SIZE_MINIMIZED)
		{
			ShowWindow(hWnd, SW_HIDE); // Прячем окно
			Shell_NotifyIcon(NIM_ADD, pNID); // Добавляем иконку в трэй
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_TIME:
			SYSTEMTIME st;
			GetLocalTime(&st);
			TCHAR BUFF2[10];
			wsprintf(BUFF2, TEXT("%d"), st.wHour);
			SetWindowText(GetDlgItem(hMainWnd,IDC_HOURS), BUFF2);
			wsprintf(BUFF2, TEXT("%d"), st.wMinute);
			SetWindowText(GetDlgItem(hMainWnd,IDC_MINUTES), BUFF2);
			wsprintf(BUFF2, TEXT("%d"), st.wSecond);
			SetWindowText(GetDlgItem(hMainWnd,IDC_SECONDS), BUFF2);
			break;
		case IDC_PATH:
			{
				TCHAR   szFile[MAX_PATH] = TEXT("\0");
				char    *szFileContent = nullptr;
				OPENFILENAME   ofn;
				HANDLE hFile = INVALID_HANDLE_VALUE;
				DWORD dwFileSize = 0, bytesToRead = 0, bytesRead = 0;
				memset( &(ofn), 0, sizeof(ofn));
				ofn.lStructSize   = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrFilter = TEXT("Exe (*.exe)\0 *.exe\0");   
				ofn.lpstrTitle = TEXT("Open File");
				ofn.Flags = OFN_EXPLORER;

				//получаем название программы которое хотим открыть
				if (GetOpenFileName(&ofn))
				{
					SetWindowText(GetDlgItem(hMainWnd,IDC_TASK), ofn.lpstrFile); 
				}
			}
			break;				
		case IDC_ADD:
			{
				AddFlag = TRUE;
				EnableTaskEdits();
				CheckButtons();
			}
			break;
		case IDC_REMOVE:
			{
				int POS = SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_GETCURSEL, 0, 0);
				if(POS != LB_ERR)
				{
					SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_DELETESTRING, POS, 0);
					TerminateThread(Tasks[POS]->hTaskThread,0);
					CloseHandle(Tasks[POS]->hTaskThread);
					Tasks.erase(Tasks.begin() + POS);			
				}
				CheckButtons();
				DisableTaskEdits();
			}
			break;
		case IDC_CHANGE:
			AddFlag = FALSE;
			EnableTaskEdits();
			break;
		case IDC_START:
			Task* NT = new Task;
			Task &NewTask = *NT;
			GetWindowText(GetDlgItem(hWnd, IDC_TASK), NewTask.cmd, 64);
			NewTask.isCompleted = FALSE;
			TCHAR BUFF[12];
			GetWindowText(GetDlgItem(hWnd, IDC_HOURS), BUFF, 64);
			NewTask.Time.hours = _tstoi(BUFF);
			GetWindowText(GetDlgItem(hWnd, IDC_MINUTES), BUFF, 64);
			NewTask.Time.minutes = _tstoi(BUFF);
			GetWindowText(GetDlgItem(hWnd, IDC_SECONDS), BUFF, 64);
			NewTask.Time.seconds = _tstoi(BUFF);
			if(AddFlag)
				NewTask.hTaskThread = CreateThread(nullptr, NULL, TaskThread, &NewTask, NULL, nullptr);
			//закроем
			if(AddFlag)
			{
				Tasks.push_back(&NewTask);
				TCHAR szTask[64], szComplete[24];
				if(NewTask.isCompleted)
					_tcscpy(szComplete, TEXT("Выполнено"));
				else
					_tcscpy(szComplete, TEXT("Не выполнено"));
				wsprintf(szTask, TEXT("%s @ %d:%d:%d %s"), NewTask.cmd, NewTask.Time.hours, NewTask.Time.minutes, NewTask.Time.seconds, szComplete);
				SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_ADDSTRING, 0, (LPARAM)szTask); 
			}
			else
			{
				INT POS = SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_GETCURSEL, 0, 0);
				TerminateThread(Tasks[POS]->hTaskThread,0);
				CancelWaitableTimer(Tasks[POS]->hTimer); // отменяем таймер
				CloseHandle(Tasks[POS]->hTimer); // закрываем дескриптор таймера
				NewTask.hTaskThread = CreateThread(nullptr, NULL, TaskThread, &NewTask, NULL, nullptr);
				Tasks[POS] = &NewTask;
				TCHAR szTask[64];
				wsprintf(szTask, TEXT("%s @ %d:%d:%d %s"), NewTask.cmd, NewTask.Time.hours, NewTask.Time.minutes, NewTask.Time.seconds, TEXT("Не выполнено"));
				SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_DELETESTRING, POS, (LPARAM)szTask);
				SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_INSERTSTRING, POS, (LPARAM)szTask); 
			}
			DisableTaskEdits();
			break;
		}
		switch(HIWORD(wParam))
		{
		case LBN_SELCHANGE:
			DisableTaskEdits();
			CheckButtons();
			break;
		}
		return TRUE;
	default:
		return DefDlgProc(nullptr, Message, wParam, lParam);
	}
	return FALSE;
}

void CheckButtons()
{
	if(SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_GETCOUNT, 0, 0))
	{
		if(SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_GETCURSEL, 0, 0) != LB_ERR)
		{
			EnableWindow(GetDlgItem(hMainWnd, IDC_REMOVE), TRUE);
			EnableWindow(GetDlgItem(hMainWnd, IDC_CHANGE), TRUE);
		}
		else
		{
			EnableWindow(GetDlgItem(hMainWnd, IDC_CHANGE), FALSE);
			EnableWindow(GetDlgItem(hMainWnd, IDC_REMOVE), FALSE);
		}
	}
	else
	{
		EnableWindow(GetDlgItem(hMainWnd, IDC_CHANGE), FALSE);
		EnableWindow(GetDlgItem(hMainWnd, IDC_REMOVE), FALSE);
	}
}

void EnableTaskEdits()
{
	EnableWindow(GetDlgItem(hMainWnd,IDC_TEXT_TASK), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TEXT_TIME), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TASK), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_START), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_HOURS), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_MINUTES), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_SECONDS), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TIME), TRUE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_PATH), TRUE);
}

void DisableTaskEdits()
{
	SetWindowText(GetDlgItem(hMainWnd,IDC_HOURS), TEXT(""));
	SetWindowText(GetDlgItem(hMainWnd,IDC_MINUTES), TEXT(""));
	SetWindowText(GetDlgItem(hMainWnd,IDC_SECONDS), TEXT(""));
	SetWindowText(GetDlgItem(hMainWnd,IDC_TASK), TEXT(""));
	EnableWindow(GetDlgItem(hMainWnd,IDC_TEXT_TASK), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TEXT_TIME), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TASK), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_START), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_HOURS), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_MINUTES), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_SECONDS), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_TIME), FALSE);
	EnableWindow(GetDlgItem(hMainWnd,IDC_PATH), FALSE);
}

DWORD WINAPI TaskThread(LPVOID lp)
{
	Task *pTask = (Task*)lp;
	pTask->hTimer = CreateWaitableTimer(nullptr, TRUE, nullptr);
	SYSTEMTIME st;
	GetLocalTime(&st); // получим текущее локальное время
	if(st.wHour > pTask->Time.hours || st.wHour == pTask->Time.hours && st.wMinute > pTask->Time.minutes || 
		st.wHour == pTask->Time.hours && st.wMinute == pTask->Time.minutes && st.wSecond > pTask->Time.seconds)
	{
		CloseHandle(pTask->hTimer);
		MessageBox(nullptr, TEXT("Я не могу выполнить это задание! Проверь настройки"), nullptr, MB_ICONWARNING| MB_OK);
		//enable?
		//pTask->isCompleted = TRUE;
		//update
		UpdateList();
		return 0;
	}
	st.wHour = pTask->Time.hours;
	st.wMinute = pTask->Time.minutes;
	st.wSecond = pTask->Time.seconds;
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft); // преобразуем структуру SYSTEMTIME в FILETIME
	LocalFileTimeToFileTime(&ft, &ft); // преобразуем местное время в UTC-время 
	SetWaitableTimer(pTask->hTimer, (LARGE_INTEGER *)&ft, 0, nullptr, nullptr, FALSE); // устанавливаем таймер
	// ожидаем переход таймера в сигнальное состояние
	if(WaitForSingleObject(pTask->hTimer, INFINITE) == WAIT_OBJECT_0) // WaitForMultipleObjects [EVENT]
	{
		Shell_NotifyIcon(NIM_DELETE, pNID); // Удаляем иконку из трэя
		ShowWindow(hMainWnd, SW_NORMAL); // Восстанавливаем окно
		SetForegroundWindow(hMainWnd); // устанавливаем окно на передний план
		//magic
		HINSTANCE hInst = ShellExecute(hMainWnd, nullptr, pTask->cmd, nullptr, nullptr, SW_SHOWNORMAL);
		if(hInst)
			pTask->isCompleted = TRUE;
		CloseHandle(hInst);
	}
	CancelWaitableTimer(pTask->hTimer); // отменяем таймер
	CloseHandle(pTask->hTimer); // закрываем дескриптор таймера
	//update
	UpdateList();
	return 0;
}

void UpdateList()
{
	SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_RESETCONTENT, 0, 0);
	for(int i = 0; i < Tasks.size(); i++)
	{
		TCHAR szTask[64], szComplete[24];
		if(Tasks[i]->isCompleted)
			_tcscpy(szComplete, TEXT("Выполнено"));
		else
			_tcscpy(szComplete, TEXT("Не выполнено"));
		wsprintf(szTask, TEXT("%s @ %d:%d:%d %s"), Tasks[i]->cmd, Tasks[i]->Time.hours, Tasks[i]->Time.minutes, Tasks[i]->Time.seconds, szComplete);
		SendMessage(GetDlgItem(hMainWnd,IDC_TASKLIST), LB_ADDSTRING, 0, (LPARAM)szTask); 
	}
}