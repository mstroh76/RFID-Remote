// Program to control Kodi via RDM6300 RFID reader
// Code Licence: CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/)
// written by m.stroh
// Install: apt-get install libcurl4-openssl-dev libconfuse-dev libhiredis-dev
// Compile: g++ -o RFID2Kodi RFID2Kodi.cpp -lhiredis -lcurl -lconfuse -Wall


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <curl/curl.h>
#include "confuse.h"
#include <sstream>
#include <iostream>
#include "redis.h"


static sig_atomic_t end = 0;
const int cnTagIDSize = 10;
const int cnTagIDSizeByte = 5; 

static void sighandler(int signo) {
	end = 1;
}

struct MemoryStruct {
	char *memory;
	size_t size;
};


class CHTTPRequest {

	private:
		CURL *curl;
		//std::string* psAnswer;
	public:
		struct MemoryStruct MemData;


	public:
		CHTTPRequest() {
			 curl_global_init(CURL_GLOBAL_ALL);
			 curl = curl_easy_init();
			MemData.memory = NULL;
			MemData.size = 0;  	

		}
		~CHTTPRequest() {
			if (curl) {
				curl_easy_cleanup(curl);
			}
			curl_global_cleanup();
			if (MemData.memory) {
				free(MemData.memory);
				MemData.memory = NULL;
				MemData.size = 0;  
			}
		}

		void ReportError(CURLcode res, const char* szCommand) {
			if (res != CURLE_OK) {
			  fprintf(stderr, "%s() failed: %s\n", szCommand, curl_easy_strerror(res));
			}
		}

 
		static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
		{
		  size_t realsize = size * nmemb;
		  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
		 
		  mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
		  if(mem->memory == NULL) {
				printf("not enough memory (realloc returned NULL)\n");
			return 0;
		  }
		 
		  memcpy(&(mem->memory[mem->size]), contents, realsize);
		  mem->size += realsize;
		  mem->memory[mem->size] = 0;
		  printf("Input: %s\n",mem->memory);
		 
		  return realsize;
		}

/*
		static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
		  size_t realsize = size * nmemb;
		  std::string* sAnswer  = (std::string*)userp;

		  sAnswer = (char*)contents;
		  std::cout<< sAnswer;

		  return realsize;
		}
*/


		bool PostHTTPCommand(const char* szUrl, const char* szPostData) {
			//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

			struct curl_slist *chunk = NULL;
			CURLcode res;

			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
			ReportError(res, "curl_easy_setopt");

			//res = curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.56.1:8080/jsonrpc");
			res = curl_easy_setopt(curl, CURLOPT_URL, szUrl);
			//res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
			res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(szPostData));
			res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPostData);

			/* send all data to this function  */ 
			MemData.memory = (char*) malloc(1);  /* will be grown as needed by realloc above */ 
			MemData.size = 0;    /* no data at this point */
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
			/* we pass our 'chunk' struct to the callback function */ 
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&MemData);
 
			res = curl_easy_perform(curl);
			ReportError(res, "curl_easy_perform");

			return (res == CURLE_OK);
		}
};


class CKodiRemote: public CHTTPRequest {
	private:
		std::stringstream sURL;

	public:
		CKodiRemote() {
		}
		void SetServer(const char* szAddress, int nPort) {
			sURL << "http://" << szAddress << ":" << nPort << "/jsonrpc";
			std::cout << "Kodi Server Request Line: " << sURL.str() << std::endl;
		}

		void PlayVideo(const char* szVideo) {
			if (!sURL.str().empty()) {
				std::stringstream sPostData;
				PostHTTPCommand(sURL.str().c_str(), "{\"jsonrpc\": \"2.0\", \"method\": \"Playlist.Clear\", \"params\":{\"playlistid\":1}, \"id\": 1}");
				sPostData <<                        "{\"jsonrpc\": \"2.0\", \"method\": \"Playlist.Add\",   \"params\":{\"playlistid\":1, \"item\" :{ \"file\" : \"" << szVideo << "\"}}, \"id\" : 1}";
				PostHTTPCommand(sURL.str().c_str(), sPostData.str().c_str());
				PostHTTPCommand(sURL.str().c_str(), "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Open\", \"params\":{\"item\":{\"playlistid\":1, \"position\" : 0}}, \"id\": 1}");
			}
		}

		void GetVideo(std::string & sVideo) {
			PostHTTPCommand(sURL.str().c_str(), "{\"jsonrpc\": \"2.0\", \"method\": \"Player.GetItem\", \"params\": { \"properties\": [\"file\"], \"playerid\": 1 }, \"id\": \"VideoGetItem\"}");
			if (MemData.memory) {
				//{"error":{"code":-32100,"message":"Failed to execute method."},"id":"VideoGetItem","jsonrpc":"2.0"}
				//{"id":"VideoGetItem","jsonrpc":"2.0","result":{"item":{"file":"D:\\Video\\Jdauny.wmv","label":"Jdauny.wmv","type":"unknown"}}} 
				char* cPos1=strstr(MemData.memory,"file\":\"");
				if (cPos1) {
					char* cPos2 = strstr(cPos1,"\",");
					if(cPos2) {
						int nlen=cPos2-cPos1 -7;
						memmove(MemData.memory, &cPos1[7], nlen);
						MemData.memory[nlen] = 0;
						//printf("Video=%s\n",MemData.memory);
						sVideo.assign(MemData.memory, nlen);
					}
				} else {
					MemData.memory[0] = 0;
					sVideo.clear();
				}
			} 
		}
};


class CConfigfile {
	private:
		cfg_t *cfg;
		long int lnKodiPort;
		char *szKodiServer;
		char *szRedisServer;
		long int lnRedisPort;
		char *szRS232Device1;
		char *szRS232Device2;

	public:

	CConfigfile() {
		szKodiServer = NULL;
		lnKodiPort = 0;
		szRedisServer = NULL;
		lnRedisPort = 0;
		szRS232Device1 = NULL;
		szRS232Device2 = NULL;

		cfg_opt_t opts[] = {
			CFG_SIMPLE_STR((char*)"kodi_server",  &szKodiServer),
			CFG_SIMPLE_INT((char*)"kodi_port",    &lnKodiPort),
			CFG_SIMPLE_STR((char*)"redis_server", &szRedisServer),
			CFG_SIMPLE_INT((char*)"redis_port",   &lnRedisPort),
			CFG_SIMPLE_STR((char*)"rs232device1", &szRS232Device1),
			CFG_SIMPLE_STR((char*)"rs232device2", &szRS232Device2),
			CFG_END()
		};
		cfg = cfg_init(opts, 0);
	}

	void LoadParametersFromFile(const char* szConfigfile) {
		cfg_parse(cfg, szConfigfile);
	}
	const char* GetKodiServer(){ return szKodiServer;};
	int GetKodiPort(){ return (int)lnKodiPort;};
	const char* GetRedisServer(){ return szRedisServer;};
	int GetRedisPort(){ return (int)lnRedisPort;};
	const char* GetRS232Device1(){ return szRS232Device1;};
	const char* GetRS232Device2(){ return szRS232Device2;};

	void FreeMemory() {
		cfg_free(cfg);
		if (szKodiServer) {
			free(szKodiServer);
			szKodiServer = NULL;
		}
		if (szRedisServer) {
			free(szRedisServer);
			szRedisServer = NULL;
		}
		if (szRS232Device1) {
			free(szRS232Device1);
			szRS232Device1 = NULL;
		}
		if (szRS232Device2) {
			free(szRS232Device2);
			szRS232Device2 = NULL;
		}
	}
	~CConfigfile() {
		FreeMemory();
	}
};


unsigned char Get2HexValue(const char* szAscii) {
	char szHex[2+1];
	unsigned int nHex;

	szHex[2] = '0';
	strncpy(szHex,  szAscii, 2);
	sscanf(szHex, "%X", &nHex); //two char to hex value
	return((unsigned char) nHex);
}


int IsChecksumOK(const char* TagID, const unsigned char* Checksum) {
	char ChecksumValue;
	char ChecksumCalc;
	int count;

	ChecksumCalc = 0;
	for (count=0; count<cnTagIDSizeByte; count++) {
		ChecksumCalc ^= Get2HexValue(&TagID[count*2]); //calc checksum as exor
	}
	ChecksumValue = Get2HexValue((const char*) Checksum);

	if (ChecksumCalc == ChecksumValue) { // compare checksum calc and transmitted
		return 1;
	} else {
		return 0;
	}
}


class CKeypressedTerminal {
	private:
		struct termios orig_term_attr, new_term_attr;

	public:
		CKeypressedTerminal(){
			tcgetattr(STDIN_FILENO, &orig_term_attr);
			memcpy(&new_term_attr, &orig_term_attr, sizeof(new_term_attr));
			new_term_attr.c_lflag &= ~(ECHO | ICANON);
			new_term_attr.c_cc[VMIN] = 0; 
			new_term_attr.c_cc[VTIME] = 0;
			tcsetattr(STDIN_FILENO, TCSANOW, &new_term_attr);
		}
		~CKeypressedTerminal(){
			tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attr);
		}
};


int main(int argc, char** argv) {
	struct termios tio;
	int tty_fd, stdin_fd;
	struct sigaction sa;
	char cRS232Sign, cKey=0;
	char TagID[cnTagIDSize+1];
 	char TagIDOld[cnTagIDSize+1];
	unsigned char Checksum[2];
	int	nPos;
	fd_set readfs;    /* file descriptor set */
	int maxfd;     /* maximum file desciptor in file descriptor set*/
	struct timeval timeout; /* timeout for wait of rs232 input */
	int select_result;
	CKeypressedTerminal KeypressedTerminal;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighandler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT,&sa, NULL);
	sigaction(SIGTERM,&sa, NULL);


	CHTTPRequest HTTPRequest;
	CKodiRemote KodiRemote;
	CRedis Redis;

	printf("RFID Kodi Remote\n");

	CConfigfile Configfile;
	Configfile.LoadParametersFromFile("remote.conf");

	KodiRemote.SetServer(Configfile.GetKodiServer(), Configfile.GetKodiPort());
	Redis.SetServer(Configfile.GetRedisServer());

	if (!Redis.Connect()) {
		printf("no connection to redis database, quit programm\n");
		return EXIT_FAILURE;
	}

	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	//CS8 = 8 bit data.
	//CREAD = Enable receiver.
	tio.c_cflag = CS8|CREAD|CLOCAL; // 8N1, see termios.h for more information
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;

	//const char cszRS232Device[] = "/dev/ttyAMA0";
	//const char cszRS232Device[] = "/dev/ttyUSB0";
	const char* cszRS232Device = Configfile.GetRS232Device1();

	stdin_fd = fileno(stdin);

	printf("Open device '%s' 9600, 8n1\n", cszRS232Device);
	tty_fd = open(cszRS232Device, O_RDWR | O_NONBLOCK);
	if (-1 == tty_fd) {
		printf("unable to open device, error '%s' (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}
	FD_ZERO(&readfs);

	cfsetospeed(&tio, B9600);            // 9600 baud
	cfsetispeed(&tio, B9600);            // 9600 baud

	tcsetattr(tty_fd, TCSANOW, &tio);

	nPos = 0;
	TagID[0] = '\0';
	TagIDOld[0] = '\0';

	do {
		/* set timeout value for rs232 input */
		timeout.tv_usec = 0;  /* milliseconds */
		timeout.tv_sec  = 1;  /* seconds */
		maxfd = tty_fd + 1;
		FD_SET(tty_fd, &readfs);  /* set wait for rs232 input*/
		FD_SET(stdin_fd, &readfs);  /* set wait for keyboard input*/

		select_result = select(maxfd, &readfs, NULL, NULL, &timeout);
		if (select_result < 0 ) {
			if (errno != EINTR) { // not signal (Ctrl+c)
				printf("select error %d\n", errno);
			}
		} else if (select_result == 0 ) { // timeout
			cKey = 0; //reset key
			if (TagID[0] != '\0') {
				printf("TagID %s gone\n", TagID);
				nPos = 0;
				TagID[0] = '\0';
				TagIDOld[0] = '\0';
			}
		} else {
			if (FD_ISSET(tty_fd, &readfs)) {
				while (read(tty_fd, &cRS232Sign, 1)>0) {
					if (cRS232Sign == 2) { // STX - Start
						nPos = 0;
						TagID[nPos] = '\0';
					} else if (cRS232Sign == 3) { // ETX - Ende
						if (IsChecksumOK(TagID, Checksum)) {
							if (strcmp(TagIDOld, TagID)) {
								printf("TagID %s found\n", TagID);  fflush(stdout);
								strcpy(TagIDOld, TagID);
								if (cKey>32) {
									switch(cKey) {
										case 'a': {
											std::string sVideo;
											KodiRemote.GetVideo(sVideo);
											if (!sVideo.empty()) {
												printf("Assign Tag %s to actual video %s\n", TagID, sVideo.c_str());
												Redis.SetValue(TagID, sVideo.c_str());
											} else {
												printf("Assign Tag %s not possible, no video active at kodi\n", TagID);
											}
											break; 
										}
										case 'r':
											printf("Remove Tag %s\n", TagID);
											Redis.Remove(TagID);
											break;
										default: 
											printf("no key"); fflush(stdout);
											break;
									}
								} else {
									std::string sVideo;
									Redis.GetValue(TagIDOld, sVideo);
									if (!sVideo.empty()) {
										printf("Play Tag %s video %s\n", TagIDOld, sVideo.c_str()); fflush(stdout);
										KodiRemote.PlayVideo(sVideo.c_str());
									} else {
										printf("Play Tag %s not possible, video empty\n", TagIDOld); fflush(stdout);
									}
								}
								cKey = 0;
							}
						} else {
							printf("TagID = %s, Checksum error!\n", TagID);
						}
					} else { //char received
						if (nPos<cnTagIDSize) {
							TagID[nPos] = cRS232Sign;
							TagID[nPos+1] = '\0';
						} else if (nPos<12) {
							Checksum[nPos-cnTagIDSize] = cRS232Sign;
						}
						nPos++;
					}
				}
			}
			if (FD_ISSET(stdin_fd, &readfs)) {
				while (read(stdin_fd, &cKey, 1)>0) {
				}
			}
		}
	} while (!end);

	printf("Close device '%s'\n", cszRS232Device);
	close(tty_fd);

	return EXIT_SUCCESS;
}
