#include "Common.h"
#include "resource.h"
#include <CommCtrl.h>
#include <atlstr.h> // mfc 사용x
#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 에디트 컨트롤 출력 함수
void DisplayText(const char* fmt, ...);
// 소켓 함수 오류 출력
void DisplayError(const char* msg);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);

SOCKET sock; // 소켓
char buf[BUFSIZE + 1]; // 데이터 송수신 버퍼
HANDLE hReadEvent, hWriteEvent; // 이벤트
HWND hSendButton; // 보내기 버튼
HWND hSearchButton; // 찾기 버튼
HWND hEdit1, hEdit2; // 에디트 컨트롤
HWND hProgress;
HWND Have;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // 이벤트 생성
    hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // 소켓 통신 스레드 생성
    CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

    // 대화상자 생성
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

    // 이벤트 제거
    CloseHandle(hReadEvent);
    CloseHandle(hWriteEvent);

    // 윈속 종료
    WSACleanup();
    return 0;
}

//void CharToWChar(const char* pstrSrc, wchar_t pwstrDest[])
//{
//   int nLen = (int)strlen(pstrSrc) + 1;
//   mbstowcs(pwstrDest, pstrSrc, nLen);
//}

wchar_t* CharToWChar(char* pstrSrc)
{
    int nLen = (int)strlen(pstrSrc) + 1;
    wchar_t* pwstr = (LPWSTR)malloc(sizeof(wchar_t) * nLen);
    mbstowcs(pwstr, pstrSrc, nLen);
    return pwstr;
}

char* WCharToChar(const wchar_t* pwstrSrc)
{
    int nLen = (int)wcslen(pwstrSrc) + 1;
    char* pstr = (char*)malloc(sizeof(char) * nLen);
    wcstombs(pstr, pwstrSrc, nLen);
    return pstr;
}

wchar_t lpstrFile[MAX_PATH];
char str[256];
OPENFILENAME OFN;
// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_INITDIALOG:

        hEdit1 = GetDlgItem(hDlg, IDC_EDIT1);
        hEdit2 = GetDlgItem(hDlg, IDC_EDIT2);
        hSearchButton = GetDlgItem(hDlg, IDC_BUTTON1);
        hSendButton = GetDlgItem(hDlg, IDC_BUTTON2);
        hProgress = GetDlgItem(hDlg, IDC_PROGRESS1);
        SendMessage(hEdit1, EM_SETLIMITTEXT, BUFSIZE, 0);
        SendMessage(hProgress, PBM_SETRANGE, 0, 100);
        SendMessage(hProgress, PBM_SETPOS, 10, 0);

        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BUTTON1:
            //SendMessage(hProgress, PBM_SETPOS, (WPARAM)nPos, 0);
            
            memset(&OFN, 0, sizeof(OPENFILENAME));
            OFN.lStructSize = sizeof(OPENFILENAME);
            OFN.hwndOwner = hDlg;
            OFN.lpstrFilter = L"Every File(*.*)\0*.*\0Text File\0*.txt;*.doc\0";
            OFN.lpstrFile = lpstrFile;
            OFN.nMaxFile = 256;
            if (GetOpenFileName(&OFN) != 0) {
                char* p = WCharToChar(OFN.lpstrFile);
                SetDlgItemTextA(hDlg, IDC_EDIT2, p);
                
            }
            SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));//프로그레스 초기화
            SendMessage(hProgress, PBM_SETPOS, 0, 0);//프로그레스 초기값
            //EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
            //WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 대기
            //SetEvent(hWriteEvent); // 쓰기 완료 알림
            //SetFocus(hEdit1); // 키보드 포커스 전환
            //SendMessage(hEdit1, EM_SETSEL, 0, -1); // 텍스트 전체 선택
            return TRUE;
        case IDC_BUTTON2:
            SetEvent(hWriteEvent); // 쓰기 완료 알림
            break;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL); // 대화상자 닫기
            closesocket(sock); // 소켓 닫기
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

// 에디트 컨트롤 출력 함수
void DisplayText(const char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    char cbuf[BUFSIZE * 2];
    vsprintf(cbuf, fmt, arg);
    va_end(arg);

    int nLength = GetWindowTextLength(hEdit2);
    SendMessage(hEdit2, EM_SETSEL, nLength, nLength);
    SendMessageA(hEdit2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
}

// 소켓 함수 오류 출력
void DisplayError(const char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char*)&lpMsgBuf, 0, NULL);
    DisplayText("[%s] %s\r\n", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
    int retval;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) err_quit("socket()");

    // connect()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("connect()");

    //// 서버와 데이터 통신
    //while (1) {
    //    WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 대기

    //    // 문자열 길이가 0이면 보내지 않음
    //    if (strlen(buf) == 0) {
    //        EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
    //        SetEvent(hReadEvent); // 읽기 완료 알림
    //        continue;
    //    }

    //    // 데이터 보내기
    //    retval = send(sock, buf, (int)strlen(buf), 0);
    //    if (retval == SOCKET_ERROR) {
    //        DisplayError("send()");
    //        break;
    //    }
    //    DisplayText("[TCP 클라이언트] %d바이트를 보냈습니다.\r\n", retval);

    //    // 데이터 받기
    //    retval = recv(sock, buf, retval, MSG_WAITALL);
    //    if (retval == SOCKET_ERROR) {
    //        DisplayError("recv()");
    //        break;
    //    }
    //    else if (retval == 0)
    //        break;

    //    // 받은 데이터 출력
    //    buf[retval] = '\0';
    //    DisplayText("[TCP 클라이언트] %d바이트를 받았습니다.\r\n", retval);
    //    DisplayText("[받은 데이터] %s\r\n", buf);

    //    EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
    //    SetEvent(hReadEvent); // 읽기 완료 알림
    //}
    while (1)
    {
        WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 대기
        FILE* fp = NULL;

        char* FileDataBuf = NULL;
        char* FileNameBuf = NULL;

        char* FilePath = WCharToChar(OFN.lpstrFile);
        
        char* FileName = strrchr(FilePath, '\\') + 1;

        const char* ip = SERVERIP;

        fp = fopen(FileName, "rb");
        if (fp == NULL) {
            printf("%s 존재하지 않는 파일명입니다.", FileName);
            return 0;
        }

        // 파일 크기 구하기
        fseek(fp, 0, SEEK_END);
        int Filesize = ftell(fp);

        fseek(fp, 0, SEEK_SET); // 파일 포인터를 파일의 처음으로 이동시킴

        // 파일 이름 구하기
        int NameSize = strlen(FileName);

        FileDataBuf = new char[Filesize];
        FileNameBuf = new char[NameSize];

        memset(FileDataBuf, 0, Filesize); // 파일 크기만큼 메모리를 0으로 초기화


        // 서버와 데이터 통신
        // 파일 이름 데이터 
        strncpy(FileNameBuf, FileName, NameSize);
        FileNameBuf[NameSize] = '\0';
        // 데이터 보내기(파일 이름 크기)
        retval = send(sock, (const char*)&NameSize, sizeof(NameSize), 0);
        if (retval == SOCKET_ERROR) {
            err_display("send() - file name size");
        }

        // 데이터 보내기(이름 데이터) // 크기
        retval = send(sock, FileNameBuf, NameSize, 0);
        if (retval == SOCKET_ERROR) {
            err_display("send() - file name buf");
        }

        // 데이터 보내기(데이터) // 파일 크기
        retval = send(sock, (const char*)&Filesize, sizeof(Filesize), 0);
        if (retval == SOCKET_ERROR) {
            err_display("send() - file size");
        }


        char dataBuf[BUFSIZE];
        int leftDataSize = Filesize; // 미 수신 데이터
        int bufSize = BUFSIZE;

        while (leftDataSize > 0) {
            if (leftDataSize < BUFSIZE)
                bufSize = leftDataSize;
            else
                bufSize = BUFSIZE;

            // 데이터 보내기 (가변 길이) - 파일 내용
            fread(dataBuf, bufSize, 1, fp);

            retval = send(sock, dataBuf, bufSize, 0);
            if (retval == SOCKET_ERROR) {
                err_display("send() file data ");
                break;
            }
            SendMessage((HWND)hProgress, PBM_SETPOS, ((float)(Filesize - leftDataSize) / Filesize) * 100, 100);
            //SendDlgItemMessage(hProgress, IDC_PROGRESS1, PBM_SETPOS, (float)leftDataSize / Filesize, 0);
            leftDataSize -= bufSize;
        }

        delete[] FileDataBuf;
        //delete[] FileNameBuf;
        fclose(fp);

    }
    return 0;
}