#ifndef _CONNECTION_REPORT_H_
#define _CONNECTION_REPORT_H_

#define MAX_PARTICIPANTS 64

enum PARTICIPANT_TYPE 
{
	PARTICIPANT_TYPE_local,
	PARTICIPANT_TYPE_remote,
	PARTICIPANT_TYPE_spectator,
};

enum CONNECTION_STATE 
{
	CONNECTION_STATE_connecting = 0,
	CONNECTION_STATE_synchronizing,
	CONNECTION_STATE_running,
	CONNECTION_STATE_disconnected,
	CONNECTION_STATE_disconnecting,
};

typedef struct ConnectionInfo
{
	enum PARTICIPANT_TYPE type;
	enum CONNECTION_STATE state;
	int connect_progress;
	int disconnect_timeout;
	int disconnect_start;
} ConnectionInfo;

typedef struct ConnectionReport
{
	char status[1024];
	int num_participants;
	ConnectionInfo participants[MAX_PARTICIPANTS];
} ConnectionReport;

#endif // ifndef _CONNECTION_REPORT_H_
