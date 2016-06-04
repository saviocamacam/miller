#include "logger.h"

Log* createLog()
{
	Log *logger = malloc(sizeof(Log));
	if( logger == NULL )
	{
		perror("Erro na criação do log!");
		exit(1);
	}

	logger->arquivoLog = tmpfile();
	logger->diagramaEventos = tmpfile();
	logger->sequenciaTermino = tmpfile();
	logger->header = tmpfile();

	fprintf(logger->sequenciaTermino, "Termino: ");
	fprintf(logger->diagramaEventos, "\nDiagrama de Eventos\n");

	return logger;
}

int recordEvent( Log *logger, char *content, LogType logtype )
{
	switch (logtype)
	{
		case DIAGRAM_EVT:
			return fprintf(logger->diagramaEventos, "%s\n", content);
		case SEQUENC_TER:
			return fprintf(logger->sequenciaTermino, "%s", content);
		case HEADER:
			return fprintf(logger->header, "%s\n", content);
		case ARQUIVO_LOG:
			break;
		default:
			break;
	}

	return 0;
}

void getLog( Log *logger, char *fname )
{
	FILE *target;

	if(fname == NULL) {
		target = stdout;
	} else {
		target = fopen(fname, "w+");
		if(target==NULL)
		{
			perror("Falha ao criar o arquivo de saida!");
			exit(1);
		}
	}
	char *content = NULL;
	size_t nbytes;
	ssize_t read = 0;

	rewind(logger->header);
	rewind(logger->sequenciaTermino);
	rewind(logger->diagramaEventos);

	// le o conteudo do header
	while((read = getline(&content, &nbytes, logger->header)) != (-1) )
		fprintf(target,"%s", content);

	// le a sequencia de termino
	while((read = getline(&content, &nbytes, logger->sequenciaTermino)) != (-1) )
		fprintf(target,"%s", content);

	// le o diagrama de eventos
	while((read = getline(&content, &nbytes, logger->diagramaEventos)) != (-1) )
		fprintf(target,"%s", content);

	free(content);
	fclose(target);

}

void freeLog( Log *logger )
{
	fclose(logger->arquivoLog);
	free(logger->arquivoLog);

	fclose(logger->diagramaEventos);
	free(logger->diagramaEventos);

	fclose(logger->sequenciaTermino);
	free(logger->sequenciaTermino);

	fclose(logger->header);
	free(logger->header);

	free(logger);
}
