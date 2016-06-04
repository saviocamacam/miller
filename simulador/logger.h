#ifndef LOGGER_H
#define LOGGER_H 1

#include<stdio.h>
#include<stdlib.h>

struct _log{
	FILE *arquivoLog;
	FILE *diagramaEventos;
	FILE *sequenciaTermino;
	FILE *header;
};

enum _logtype {
	DIAGRAM_EVT,
	SEQUENC_TER,
	HEADER,
	ARQUIVO_LOG
};

typedef struct _log Log;
typedef enum _logtype LogType;

Log* createLog();

int recordEvent( Log *logger, char *content, LogType logtype );

void getLog( Log *logger, char *fname );

void freeLog( Log *logger );

#endif
