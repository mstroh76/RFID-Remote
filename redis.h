#include <string>
#include <hiredis/hiredis.h>
#include <string.h>

// http://redis.io/commands

class CRedis{
private:
	int nPort;
	std::string sServer;
	redisContext *rc;
	redisReply *reply;

public:
	CRedis() {
		nPort = 6379;
		rc = NULL;
		reply = NULL;
	}	
	
	~CRedis(){
		if(rc) {
			redisFree(rc);
		}	
	}
	
	void SetServer(const char* szServer) {
		sServer = szServer;
	}
	
	bool Connect() {
	    printf("Connecting to redis server at %s:%d... ",sServer.c_str(), nPort);
        struct timeval timeout = { 2, 500000 }; // 2.5 seconds
        rc = redisConnectWithTimeout(sServer.c_str(), nPort, timeout);
        if (rc == NULL || rc->err) {
            if (rc) {
                printf("error: %s\n", rc->errstr);
                redisFree(rc);
                rc = NULL;
            } else {
				printf("error: can't allocate redis context\n");
            }
            return false;
        } else {
			printf("success\n");
        }

        // PING server
        reply = (redisReply *) redisCommand(rc, "PING");
        printf("Command PING: %s\n", reply->str);
        
        return true;
	}
	
	void GetValue(char* szKey, std::string &sValue) {
		//printf("\n Get Redis key %s ...", szKey); fflush(stdout);
		sValue.clear();
		if (szKey && strlen(szKey)>0) {
			reply = (redisReply *) redisCommand(rc, "GET %s", szKey);
			if (reply->str) {
					printf("Redis: Value: %s\n", reply->str); fflush(stdout);
					sValue.assign(reply->str);
			} else {
					printf("Redis: Error: key not found\n"); fflush(stdout);
			}
			//freeReplyObject(reply);
		} else {
			printf("Redis: Error: Parameter invalid\n"); fflush(stdout);
		}
		
	}
	
	void SetValue(const char* szKey, const char* szValue) {
	  if (szValue && szKey && strlen(szValue)>0 && strlen(szKey)>0) {
			//printf("Command SET: store link for card-id '%s' with value '%s'\n", szKey, szValue);
			reply = (redisReply *)redisCommand(rc, "SET %s %s", szKey, szValue);
			printf("Redis: Command SET reply: %s\n", reply->str);
			//freeReplyObject(reply);
		} else {
			printf("Error: Parameter invalid\n");
		}  		
	}
	
	void Remove(const char* szKey) {
	  if (szKey && strlen(szKey)>0) {
			//printf("Command DEL: remove card-id '%s' \n", szKey);
			reply = (redisReply *)redisCommand(rc, "DEL %s", szKey);
			printf("Redis: Command DEL reply: %s\n", reply->str);
			//freeReplyObject(reply);
		} else {
			printf("Error: Parameter invalid\n");
		}  		
	}
};

