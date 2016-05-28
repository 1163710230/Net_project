
//#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include <fstream>
#include <map>
#include<string>

#include<iostream>
using namespace std;

#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80 //http �������˿�

//Http ��Ҫͷ������
struct HttpHeader {
	char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
	char url[1024];  // ����� url
	char host[1024]; // Ŀ������
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};

//cache �洢���ݽṹ
map<string, char *>cache;
struct HttpCache {
	char url[1024];
	char host[1024];
	char last_modified[200];
	char status[4];
	char buffer[MAXSIZE];
	HttpCache() {
		ZeroMemory(this, sizeof(HttpCache));
	}
};
HttpCache Cache[1024];
int cached_number = 0;//�Ѿ������url��
int last_cache = 0;//��һ�λ��������

//�û�����
char ForbiddenIP[1024][17];
int IPnum = 0;

//������վ
char fishUrl[1024][1024];
int fishUrlnum=0;

BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
int ParseHttpHead0(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
bool ForbiddenToConnect(char *httpheader);
bool GotoFalseWebsite(char *url);
void ParseCache(char *buffer, char *status, char* last_modified);
bool UserIsForbidden(char *userID);  //�û�����
//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
//�����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};
int _tmain(int argc, _TCHAR* argv[])
{
	printf("�����������������\n");
	printf("��ʼ��...\n");
	if (!InitSocket()) {
		printf("socket ��ʼ��ʧ��\n");
		return -1;
	}
	printf("����������������У������˿� %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	SOCKADDR_IN acceptAddr;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//������������ϼ���
	char client_IP[16];
	//���ý���IP
	memcpy(ForbiddenIP[IPnum++], "204.204.204.204", 16);
	//���÷�����Щ��վ�ᱻ�ض��򵽵�����վ
	memcpy(fishUrl[fishUrlnum++], "http://www.asus.com.cn/",23);
	memcpy(fishUrl[fishUrlnum++],"http://pku.edu.cn/",18);
	while (true) {
		acceptSocket = accept(ProxyServer, (SOCKADDR*)&acceptAddr, NULL);
		//printf("��ȡ�û�IP��ַ��%s\n",inet_ntoa(acceptAddr.sin_addr));
		memcpy(client_IP, inet_ntoa(acceptAddr.sin_addr),16);
		//�����û�IP����
		/*
		if (UserIsForbidden(client_IP))
		{
			printf("IP������\n");
			closesocket(acceptSocket);
		}*/
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0,
			&ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(500);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket() {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
		printf("���� winsock ʧ�ܣ� �������Ϊ: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ�� winsock �汾\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("�����׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("���׽���ʧ��\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("�����˿�%d ʧ��", ProxyPort);
		return FALSE;
	}
	return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	char Buffer[MAXSIZE];
	ZeroMemory(Buffer, MAXSIZE);
	char sendBuffer[MAXSIZE];
	ZeroMemory(sendBuffer, MAXSIZE);
	char FishBuffer[MAXSIZE];
	ZeroMemory(FishBuffer, MAXSIZE);
	char *CacheBuffer;
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	int Have_cache;
	//cache �������
	char *cacheBuffer0 = new char[MAXSIZE];
	char *p;
	map<string, char *>::iterator iter;
	string sp;
	//���տͻ��˵�����
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
	}
	//printf("�ͻ��������� buffer \n %s\n", Buffer);
	memcpy(sendBuffer, Buffer, recvSize);
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	//ParseHttpHead(CacheBuffer, httpHeader);
	Have_cache = ParseHttpHead0(CacheBuffer, httpHeader);
	delete CacheBuffer;

	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}
	printf("������������ %s �ɹ�\n", httpHeader->host);
	//���ν�����վ��Ϣ
	/*if (!ForbiddenToConnect(httpHeader->url))
	{
		printf("��������� %s \n",httpHeader->url);
		goto error;
		
	}*/
	//��վ����  ����pku.edu.cn  asus.com �ض��򵽽��չ�����
	if (GotoFalseWebsite(httpHeader->url))
	{
		
		char* pr;
		int fishing_len = strlen("HTTP/1.1 302 Moved Temporarily\r\n");
		memcpy(FishBuffer, "HTTP/1.1 302 Moved Temporarily\r\n", fishing_len);
		pr = FishBuffer + fishing_len;
		fishing_len = strlen("Connection:keep-alive\r\n");
		memcpy(pr, "Connection:keep-alive\r\n", fishing_len);
		pr = pr + fishing_len;
		fishing_len = strlen("Cache-Control:max-age=0\r\n");
		memcpy(pr, "Cache-Control:max-age=0\r\n", fishing_len);
		pr = pr + fishing_len;
		//�ض��򵽽��չ�����
		fishing_len = strlen("Location: http://today.hit.edu.cn/\r\n\r\n");
		memcpy(pr, "Location: http://today.hit.edu.cn/\r\n\r\n", fishing_len);
		//��302���ķ��ظ��ͻ���
		ret = send(((ProxyParam*)lpParameter)->clientSocket, FishBuffer, sizeof(FishBuffer), 0);
		goto error;
	}

	//cache ʵ��
	if (Have_cache) //�����л���
	//if(false)
	{
		char cached_buffer[MAXSIZE];
		ZeroMemory(cached_buffer, MAXSIZE);
		memcpy(cached_buffer, Buffer, recvSize);

		//���컺��ı���ͷ
		char* pr = cached_buffer + recvSize;
		memcpy(pr, "If-modified-since: ", 19);
		pr += 19;
		int lenth = strlen(Cache[last_cache].last_modified);
		memcpy(pr, Cache[last_cache].last_modified, lenth);
		pr += lenth;

		//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
		ret = send(((ProxyParam *)lpParameter)->serverSocket, cached_buffer, strlen(cached_buffer) + 1, 0);
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, cached_buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			goto error;
		}

		//��������������Ϣ��HTTP����ͷ
		CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, cached_buffer, recvSize);
		char last_status[4];//���ڼ�¼�������ص�״̬��
		char last_modified[30];//���ڼ�¼��ס���ص�ҳ���޸ĵ�ʱ��
		ParseCache(CacheBuffer, last_status, last_modified);
		delete CacheBuffer;

		//����cache��״̬��
		if (strcmp(last_status, "304") == 0) {//û�б��޸�
			printf("ҳ��û���޸Ĺ�,�����urlΪ:%s\n", Cache[last_cache].url);
			//�����������ֱ��ת�����ͻ���
			ret = send(((ProxyParam*)lpParameter)->clientSocket, Cache[last_cache].buffer, sizeof(Cache[last_cache].buffer), 0);
			if (ret != SOCKET_ERROR)
			{
				printf("���Ի���++++++++++++++\n");
			}
		}
		else if (strcmp(last_status, "200") == 0) {//�Ѿ��޸���
												   //�޸Ļ����е�����
			printf("ҳ�汻�޸Ĺ�,�����urlΪ:%s\n", Cache[last_cache].url);
			memcpy(Cache[last_cache].buffer, cached_buffer, strlen(cached_buffer));
			memcpy(Cache[last_cache].last_modified, last_modified, strlen(last_modified));

			//��Ŀ����������ص�����ֱ��ת�����ͻ���
			ret = send(((ProxyParam*)lpParameter)->clientSocket, cached_buffer, sizeof(cached_buffer), 0);
			if (ret != SOCKET_ERROR)
			{
				printf("�����޸Ĺ��Ļ���-----------\n");
			}
		}
	}
	else //û�л�������ҳ��
	{
		//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
		if (ret != SOCKET_ERROR)
		{
			printf("�ɹ����͸�Ŀ��������ı���buffer \n \n");
		}
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize == SOCKET_ERROR) 
		{
			printf("Ŀ�������δ��������\n");
			goto error;
		}
		//���浽cache��

		//��Ŀ����������ص�����ֱ��ת�����ͻ���
		ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		if (ret != SOCKET_ERROR)
		{
			printf("���Է�����************\n�ɹ����͸��ͻ��˵ı���(Ŀ����������ص�)buffer ret = %d \n", ret);
		}
	}
	//������
error:
	printf("�ر��׽���\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;

}
//************************************
//Method : ForbiddenToConnect
//FullName: ForbiddenToConnect
//Access: public
//Return : bool
//Qualifier:ʵ����վ���ˣ����������ĳЩ��վ
//Parameter: char *httpheader
//************************************
bool ForbiddenToConnect(char *httpheader)
{
	char * forbiddernUrl = ".edu.cn";
	if (strstr(httpheader, forbiddernUrl)!=NULL)
	{
		return false;
	}
	else return true;
}

//************************************
//Method : UserIsForbidden
//FullName: UserIsForbidden
//Access: public
//Return : bool
//Qualifier:ʵ���û����ˣ�����IP
//Parameter: char *userID
//************************************
bool UserIsForbidden(char *userID)
{
	for (int i = 0; i < IPnum; i++)
	{
		if (strcmp(userID,ForbiddenIP[i])==0)
		{
			//�û�IP�ڽ���IP����
			return true;
		}
	}
	return false;
}
//************************************
//Method : GotoFalseWebsite
//FullName: GotoFalseWebsite
//Access: public
//Return : bool
//Qualifier:ʵ�ַ���������ģ����վ
//Parameter: char *url
//************************************
bool GotoFalseWebsite(char *url)
{
	cout << url << endl;
	for (int i = 0; i < fishUrlnum; i++)
	{
		if (strcmp(url,fishUrl[i])==0)
		{
			return true;
		}
	}
	return false;
}

//*************************
//Method: ParseCache
//FullName: ParseCache
//Access: public
//Returns: void
//Qualifier: ���� TCP �����е� HTTP ͷ��,���Ѿ�cache���е�ʱ��ʹ��
//Parameter: char *buffer
//Parameter: char * status
//Parameter: HttpHeader *httpHeader
//*************************
void ParseCache(char *buffer, char *status, char* last_modified) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
	memcpy(status, &p[9], 3);
	status[3] = '\0';
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		if (strstr(p, "Last-Modified") != NULL) {
			memcpy(last_modified, &p[15], strlen(p) - 15);
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer, HttpHeader * httpHeader) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��   ����ptr��û���˵�һ��
									  //printf("��ȡ����p = %s\n", p);
	if (p[0] == 'G') {//GET ��ʽ
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST ��ʽ
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

//*************************
//Method: ParseHttpHead0
//FullName: ParseHttpHead0
//Access: public
//Returns: void
//Qualifier: ���� TCP �����е� HTTP ͷ��
//Parameter: char *buffer
//Parameter: HttpHeader *httpHeader
//*************************

int ParseHttpHead0(char *buffer, HttpHeader *httpHeader) {
	int flag = 0;//���ڱ�ʾCache�Ƿ����У�����Ϊ1��������Ϊ0
	char *p;
	char *ptr;
	const char *delim = "\r\n";//�س����з�							 
	p = strtok_s(buffer, delim, &ptr);
	if (p[0] == 'G') {	//GET��ʽ
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
		printf("url��%s\n", httpHeader->url);//url										
		for (int i = 0; i < 1024; i++) {//����cache������ǰ���ʵ�url�Ƿ��Ѿ�����cache����
			if (strcmp(Cache[i].url, httpHeader->url) == 0) {//˵��url��cache���Ѿ�����
				flag = 1;
				break;
			}
		}
		if (!flag && cached_number != 1023) {//˵��urlû����cache��cacheû����, �����urlֱ�Ӵ��ȥ
			memcpy(Cache[cached_number].url, &p[4], strlen(p) - 13);
			last_cache = cached_number;
		}
		else if (!flag && cached_number == 1023) {//˵��urlû����cache��cache����,�ѵ�һ��cache����
			memcpy(Cache[0].url, &p[4], strlen(p) - 13);
			last_cache = 0;
		}
	}
	else if (p[0] == 'P') {	//POST��ʽ
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
		for (int i = 0; i < 1024; i++) {
			if (strcmp(Cache[i].url, httpHeader->url) == 0) {
				flag = 1;
				break;
			}
		}
		if (!flag && cached_number != 1023) {
			memcpy(Cache[cached_number].url, &p[5], strlen(p) - 14);
			last_cache = cached_number;
		}
		else if (!flag && cached_number == 1023) {
			memcpy(Cache[0].url, &p[4], strlen(p) - 13);
			last_cache = 0;
		}
	}

	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://HOST
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			if (!flag && cached_number != 1023) {
				memcpy(Cache[last_cache].host, &p[6], strlen(p) - 6);
				cached_number++;
			}
			else if (!flag && cached_number == 1023) {
				memcpy(Cache[last_cache].host, &p[6], strlen(p) - 6);
			}
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
			//case '':
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	return flag;
}

//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}