#ifndef _NON_GAMESTATE_H_
#define _NON_GAMESTATE_H_

#define MAX_REMOTES 64

typedef enum ConnectionType {
    PLAYER_TYPE_Local,
    PLAYER_TYPE_Remote,
    PLAYER_TYPE_Spectator,
} ConnectionType;

typedef enum ConnectionState {
   CONNECTION_STATE_Connecting = 0,
   CONNECTION_STATE_Synchronizing,
   CONNECTION_STATE_Running,
   CONNECTION_STATE_Disconnected,
   CONNECTION_STATE_Disconnecting,
} ConnectionState;

typedef struct ConnectionInfo {
   ConnectionType type;
   ConnectionState state;
   int connect_progress;
   int disconnect_timeout;
   int disconnect_start;
} ConnectionInfo;

typedef struct ConnectionReport {
   int num_players;
   ConnectionInfo players[MAX_REMOTES];
} ConnectionReport;

#endif
